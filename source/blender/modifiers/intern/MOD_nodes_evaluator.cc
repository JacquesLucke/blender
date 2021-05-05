/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "MOD_nodes_evaluator.hh"

#include "NOD_geometry_exec.hh"
#include "NOD_type_conversions.hh"

#include "DEG_depsgraph_query.h"

#include "FN_generic_value_map.hh"
#include "FN_multi_function.hh"

#include "BLI_stack.hh"
#include "BLI_vector_set.hh"

#include <atomic>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/task_group.h>

namespace blender::modifiers::geometry_nodes {

using bke::PersistentCollectionHandle;
using bke::PersistentObjectHandle;
using fn::CPPType;
using fn::GValueMap;
using nodes::GeoNodeExecParams;
using namespace fn::multi_function_types;

enum class ValueUsage {
  /* The value is definitely used. */
  Yes,
  /* The value may be used. */
  Maybe,
  /* The value will definitely not be used. */
  No,
};

struct SingleInputValue {
  std::atomic<void *> value = nullptr;
};

struct MultiInputValueItem {
  DSocket origin;
  void *value;
};

struct MultiInputValue {
  /* Used when appending and checking if everything has been appended. */
  std::mutex mutex;

  Vector<MultiInputValueItem> items;
  int expected_size;
};

struct InputState {
  /**
   * How the node intends to use this input.
   */
  std::atomic<ValueUsage> usage = ValueUsage::Maybe;

  const CPPType *type = nullptr;

  union {
    SingleInputValue *single;
    MultiInputValue *multi;
  } value;

  /**
   * True when this input is/was used for an evaluation. While a node is running, only the inputs
   * that have this set to true are allowed to be used. This makes sure that inputs created while
   * the node is running correctly trigger the node to run again.
   */
  std::atomic<bool> was_ready_for_evaluation = false;
};

struct OutputState {
  /**
   * If this output has been computed and forwarded already.
   */
  bool has_been_computed = false;

  /**
   * Before evaluation starts, this is set to the proper value.
   */
  std::atomic<ValueUsage> output_usage = ValueUsage::Maybe;

  ValueUsage output_usage_for_evaluation;
};

struct NodeState {
  Array<InputState> inputs;
  Array<OutputState> outputs;
  int runs = 0;

  std::mutex mutex;
  bool is_scheduled = false;
  bool is_running = false;
  bool reschedule_after_run = false;
};

class NewGeometryNodesEvaluator;

static DInputSocket get_input_by_identifier(const DNode node, const StringRef identifier)
{
  for (const InputSocketRef *socket : node->inputs()) {
    if (socket->identifier() == identifier) {
      return {node.context(), socket};
    }
  }
  return {};
}

static DOutputSocket get_output_by_identifier(const DNode node, const StringRef identifier)
{
  for (const OutputSocketRef *socket : node->outputs()) {
    if (socket->identifier() == identifier) {
      return {node.context(), socket};
    }
  }
  return {};
}

class NewNodeParamsProvider : public nodes::GeoNodeExecParamsProvider {
 private:
  NewGeometryNodesEvaluator &evaluator_;
  NodeState *node_state_;

 public:
  NewNodeParamsProvider(NewGeometryNodesEvaluator &evaluator, DNode dnode);

  bool can_get_input(StringRef identifier) const override;
  bool can_set_output(StringRef identifier) const override;
  GMutablePointer extract_input(StringRef identifier) override;
  Vector<GMutablePointer> extract_multi_input(StringRef identifier) override;
  GPointer get_input(StringRef identifier) const override;
  GMutablePointer alloc_output_value(const CPPType &type) override;
  void set_output(StringRef identifier, GMutablePointer value) override;
  void require_input(StringRef identifier) override;
  void set_input_unused(StringRef identifier) override;
};

class NewGeometryNodesEvaluator {
 private:
  LinearAllocator<> &main_allocator_;
  tbb::enumerable_thread_specific<LinearAllocator<>> local_allocators_;
  Vector<DInputSocket> group_outputs_;
  blender::nodes::MultiFunctionByNode &mf_by_node_;
  const blender::nodes::DataTypeConversions &conversions_;
  const PersistentDataHandleMap &handle_map_;
  const Object *self_object_;
  const ModifierData *modifier_;
  Depsgraph *depsgraph_;
  LogSocketValueFn log_socket_value_fn_;

  Map<DNode, NodeState *> node_states_;
  tbb::task_group task_group_;

  friend NewNodeParamsProvider;

 public:
  NewGeometryNodesEvaluator(GeometryNodesEvaluationParams &params)
      : main_allocator_(params.allocator),
        group_outputs_(std::move(params.output_sockets)),
        mf_by_node_(*params.mf_by_node),
        conversions_(blender::nodes::get_implicit_type_conversions()),
        handle_map_(*params.handle_map),
        self_object_(params.self_object),
        modifier_(&params.modifier_->modifier),
        depsgraph_(params.depsgraph),
        log_socket_value_fn_(std::move(params.log_socket_value_fn))
  {
  }

  void execute()
  {
    this->create_states_for_reachable_nodes();
    this->schedule_initial_nodes();
    // Create node states for reachable nodes.
    // Set initial required sockets, which should spawn tasks.
    task_group_.wait();
    this->free_states();
  }

  void create_states_for_reachable_nodes()
  {
    Stack<DNode> nodes_to_check;
    for (const DInputSocket &socket : group_outputs_) {
      nodes_to_check.push(socket.node());
    }
    while (!nodes_to_check.is_empty()) {
      const DNode node = nodes_to_check.pop();
      if (node_states_.contains(node)) {
        continue;
      }
      NodeState &node_state = *main_allocator_.construct<NodeState>().release();
      node_state.inputs.reinitialize(node->inputs().size());
      node_state.outputs.reinitialize(node->outputs().size());

      for (const int i : node->inputs().index_range()) {
        InputState &input_state = node_state.inputs[i];
        const InputSocketRef &socket_ref = node->input(i);
        if (!socket_ref.is_available()) {
          continue;
        }
        const CPPType *type = nodes::socket_cpp_type_get(*socket_ref.typeinfo());
        input_state.type = type;
        if (type == nullptr) {
          continue;
        }
        if (socket_ref.is_multi_input_socket()) {
          input_state.value.multi = main_allocator_.construct<MultiInputValue>().release();
          int count = 0;
          const DInputSocket socket{node.context(), &socket_ref};
          socket.foreach_origin_socket([&](DSocket UNUSED(origin)) { count++; });
          input_state.value.multi->expected_size = count;
        }
        else {
          input_state.value.single = main_allocator_.construct<SingleInputValue>().release();
        }
      }

      node_states_.add_new(node, &node_state);

      for (const InputSocketRef *input_ref : node->inputs()) {
        const DInputSocket input{node.context(), input_ref};
        input.foreach_origin_socket(
            [&](const DSocket origin) { nodes_to_check.push(origin.node()); });
      }
    }
  }

  void free_states()
  {
    for (auto &&item : node_states_.items()) {
      const DNode node = item.key;
      NodeState &node_state = *item.value;

      for (const int i : node->inputs().index_range()) {
        InputState &input_state = node_state.inputs[i];
        const InputSocketRef &socket_ref = node->input(i);
        if (socket_ref.is_multi_input_socket()) {
          MultiInputValue &multi_value = *input_state.value.multi;
          for (MultiInputValueItem &item : multi_value.items) {
            if (item.value != nullptr) {
              input_state.type->destruct(item.value);
            }
          }
          multi_value.~MultiInputValue();
        }
        else {
          SingleInputValue &single_value = *input_state.value.single;
          void *value = single_value.value.load(std::memory_order_acquire);
          if (value != nullptr) {
            input_state.type->destruct(value);
          }
          single_value.~SingleInputValue();
        }
      }

      node_state.~NodeState();
    }
  }

  void schedule_initial_nodes()
  {
    for (const DInputSocket &socket : group_outputs_) {
      this->set_input_required(socket);
    }
  }

  void set_input_required(DInputSocket socket)
  {
    const DNode node = socket.node();
    NodeState &node_state = *node_states_.lookup(node);
    InputState &input_state = node_state.inputs[socket->index()];

    if (input_state.was_ready_for_evaluation.load(std::memory_order_acquire)) {
      /* Value is used already. */
      return;
    }

    const ValueUsage old_usage = input_state.usage.load(std::memory_order_acquire);
    if (old_usage == ValueUsage::Yes) {
      /* Was already required, nothing to do. */
      return;
    }

    /* TODO: Separate handling for multi input? */
    input_state.usage.store(ValueUsage::Yes, std::memory_order_release);
    socket.foreach_origin_socket([&, this](const DSocket origin_socket) {
      if (origin_socket->is_input()) {
        /* These sockets are handled separately. */
        return;
      }
      const DNode origin_node = origin_socket.node();
      NodeState &origin_node_state = *this->node_states_.lookup(node);
      OutputState &origin_socket_state = origin_node_state.outputs[origin_socket->index()];

      if (origin_socket_state.output_usage.load(std::memory_order_acquire) != ValueUsage::Yes) {
        /* Output is marked as required already. */
        return;
      }

      origin_socket_state.output_usage.store(ValueUsage::Yes, std::memory_order_release);
      this->schedule_node_if_necessary(origin_node);
    });
  }

  void set_input_unused(DInputSocket socket)
  {
  }

  void forward_output(DOutputSocket socket, GMutablePointer value)
  {
  }

  void schedule_node_if_necessary(DNode node)
  {
    NodeState &node_state = *node_states_.lookup(node);
    std::lock_guard lock{node_state.mutex};
    if (node_state.is_running) {
      /* The node is currently running, it might have to run again because new inputs info is
       * available. */
      node_state.reschedule_after_run = true;
      return;
    }
    if (node_state.is_scheduled) {
      /* The node is scheduled already and is not running. Therefore the latest info will be taken
       * into account when it runs next. No need to schedule it again. */
    }
    /* The node is not running and was not scheduled before. Just schedule it now. */
    node_state.is_scheduled = true;
    this->add_node_to_task_group(node);
  }

  void add_node_to_task_group(DNode node)
  {
    task_group_.run([this, node]() { this->run_task(node); });
  }

  void run_task(const DNode node)
  {
    NodeState &node_state = *node_states_.lookup(node);
    {
      std::lock_guard lock{node_state.mutex};
      node_state.is_scheduled = false;
      node_state.is_running = true;
    }

    if (node_state.runs == 0) {
      this->load_unlinked_inputs(node);
    }

    bool all_required_inputs_available = true;
    for (const int i : node_state.inputs.index_range()) {
      InputState &input_state = node_state.inputs[i];
      const InputSocketRef &socket_ref = node->input(i);

      if (input_state.usage.load(std::memory_order_acquire) != ValueUsage::Yes) {
        continue;
      }
      if (socket_ref.is_multi_input_socket()) {
        MultiInputValue &multi_value = *input_state.value.multi;
        std::lock_guard lock{multi_value.mutex};
        if (multi_value.items.size() < multi_value.expected_size) {
          all_required_inputs_available = false;
          break;
        }
      }
      else {
        SingleInputValue &single_value = *input_state.value.single;
        void *value = single_value.value.load(std::memory_order_acquire);
        if (value == nullptr) {
          all_required_inputs_available = false;
          break;
        }
      }
      /* Even if the node is not evaluated in this task, it will be scheduled again by another node
       * when it provides a missing input. */
      input_state.was_ready_for_evaluation.store(true, std::memory_order_release);
    }

    if (all_required_inputs_available) {
      bool evaluation_is_necessary = false;
      for (OutputState &output_state : node_state.outputs) {
        output_state.output_usage_for_evaluation = output_state.output_usage.load(
            std::memory_order_acquire);
        if (output_state.output_usage_for_evaluation == ValueUsage::Yes) {
          if (!output_state.has_been_computed) {
            /* Only evaluate when there is an output that is required but has not been computed.
             */
            evaluation_is_necessary = true;
          }
        }
      }

      if (evaluation_is_necessary) {
        NewNodeParamsProvider params_provider{*this, node};
        GeoNodeExecParams params{params_provider};
        node->typeinfo()->geometry_node_execute(params);
      }
    }

    node_state.runs++;

    {
      std::lock_guard lock{node_state.mutex};
      node_state.is_running = false;
      if (node_state.reschedule_after_run) {
        node_state.reschedule_after_run = false;
        node_state.is_scheduled = true;
        this->add_node_to_task_group(node);
      }
    }
  }

  void load_unlinked_inputs(const DNode node)
  {
    NodeState &node_state = *node_states_.lookup(node);
    for (const InputSocketRef *input_socket_ref : node->inputs()) {
      if (!input_socket_ref->is_available()) {
        continue;
      }

      InputState &input_state = node_state.inputs[input_socket_ref->index()];

      const DInputSocket input_socket{node.context(), input_socket_ref};
      Vector<DSocket> origin_sockets;
      input_socket.foreach_origin_socket([&](DSocket origin) { origin_sockets.append(origin); });

      const CPPType &type = *blender::nodes::socket_cpp_type_get(*input_socket_ref->typeinfo());

      if (input_socket->is_multi_input_socket()) {
        MultiInputValue &multi_value = *input_state.value.multi;
        std::lock_guard lock{multi_value.mutex};
        for (const DSocket &origin : origin_sockets) {
          if (origin->is_input()) {
            GMutablePointer value = this->get_unlinked_input_value(DInputSocket(origin), type);
            multi_value.items.append({origin, value.get()});
          }
        }
      }
      else {
        SingleInputValue &single_value = *input_state.value.single;
        if (origin_sockets.is_empty()) {
          GMutablePointer value = this->get_unlinked_input_value(input_socket, type);
          single_value.value.store(value.get(), std::memory_order_release);
        }
        else {
          BLI_assert(origin_sockets.size() == 1);
          const DSocket origin = origin_sockets[0];
          if (origin->is_input()) {
            GMutablePointer value = this->get_unlinked_input_value(DInputSocket(origin), type);
            single_value.value.store(value.get(), std::memory_order_release);
          }
        }
      }
    }
  }

  GMutablePointer get_unlinked_input_value(const DInputSocket &socket,
                                           const CPPType &required_type)
  {
    LinearAllocator<> &allocator = local_allocators_.local();

    bNodeSocket *bsocket = socket->bsocket();
    const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket->typeinfo());
    void *buffer = allocator.allocate(type.size(), type.alignment());

    if (bsocket->type == SOCK_OBJECT) {
      Object *object = socket->default_value<bNodeSocketValueObject>()->value;
      PersistentObjectHandle object_handle = handle_map_.lookup(object);
      new (buffer) PersistentObjectHandle(object_handle);
    }
    else if (bsocket->type == SOCK_COLLECTION) {
      Collection *collection = socket->default_value<bNodeSocketValueCollection>()->value;
      PersistentCollectionHandle collection_handle = handle_map_.lookup(collection);
      new (buffer) PersistentCollectionHandle(collection_handle);
    }
    else {
      blender::nodes::socket_cpp_value_get(*bsocket, buffer);
    }

    if (type == required_type) {
      return {type, buffer};
    }
    if (conversions_.is_convertible(type, required_type)) {
      void *converted_buffer = allocator.allocate(required_type.size(), required_type.alignment());
      conversions_.convert_to_uninitialized(type, required_type, buffer, converted_buffer);
      type.destruct(buffer);
      return {required_type, converted_buffer};
    }
    void *default_buffer = allocator.allocate(required_type.size(), required_type.alignment());
    required_type.copy_to_uninitialized(required_type.default_value(), default_buffer);
    return {required_type, default_buffer};
  }
};

NewNodeParamsProvider::NewNodeParamsProvider(NewGeometryNodesEvaluator &evaluator, DNode dnode)
    : evaluator_(evaluator)
{
  this->dnode = dnode;
  this->handle_map = &evaluator.handle_map_;
  this->self_object = evaluator.self_object_;
  this->modifier = evaluator.modifier_;
  this->depsgraph = evaluator.depsgraph_;

  node_state_ = evaluator.node_states_.lookup(dnode);
}

bool NewNodeParamsProvider::can_get_input(StringRef identifier) const
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);

  InputState &input_state = node_state_->inputs[socket->index()];
  if (!input_state.was_ready_for_evaluation.load(std::memory_order_acquire)) {
    return false;
  }
  /* TODO: Multi input. */
  /* Check if the value has been extracted before. */
  return input_state.value.single->value.load(std::memory_order_acquire) != nullptr;
}

bool NewNodeParamsProvider::can_set_output(StringRef identifier) const
{
  const DOutputSocket socket = get_output_by_identifier(this->dnode, identifier);
  BLI_assert(socket);

  OutputState &output_state = node_state_->outputs[socket->index()];
  return !output_state.has_been_computed;
}

GMutablePointer NewNodeParamsProvider::extract_input(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);
  BLI_assert(!socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_->inputs[socket->index()];
  SingleInputValue &single_value = *input_state.value.single;
  void *value = single_value.value.load(std::memory_order_acquire);
  single_value.value.store(nullptr, std::memory_order_release);
  return {*input_state.type, value};
}

Vector<GMutablePointer> NewNodeParamsProvider::extract_multi_input(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);
  BLI_assert(socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_->inputs[socket->index()];
  MultiInputValue &multi_value = *input_state.value.multi;
  std::lock_guard lock(multi_value.mutex);

  Vector<GMutablePointer> ret_values;
  socket.foreach_origin_socket([&](DSocket origin) {
    for (const MultiInputValueItem &item : multi_value.items) {
      if (item.origin == origin) {
        ret_values.append(item.value);
        return;
      }
    }
    BLI_assert_unreachable();
  });
  multi_value.items.clear();
  return ret_values;
}

GPointer NewNodeParamsProvider::get_input(StringRef identifier) const
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);
  BLI_assert(!socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_->inputs[socket->index()];
  SingleInputValue &value = *input_state.value.single;
  return {*input_state.type, value.value.load(std::memory_order_acquire)};
}

GMutablePointer NewNodeParamsProvider::alloc_output_value(const CPPType &type)
{
  LinearAllocator<> &allocator = evaluator_.local_allocators_.local();
  return {type, allocator.allocate(type.size(), type.alignment())};
}

void NewNodeParamsProvider::set_output(StringRef identifier, GMutablePointer value)
{
  const DOutputSocket socket = get_output_by_identifier(this->dnode, identifier);
  BLI_assert(socket);

  OutputState &output_state = node_state_->outputs[socket->index()];
  BLI_assert(!output_state.has_been_computed);
  output_state.has_been_computed = true;
  evaluator_.forward_output(socket, value);
}

void NewNodeParamsProvider::require_input(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  evaluator_.set_input_required(socket);
}

void NewNodeParamsProvider::set_input_unused(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  evaluator_.set_input_unused(socket);
}

void evaluate_geometry_nodes(GeometryNodesEvaluationParams &params)
{
  NewGeometryNodesEvaluator evaluator{params};
  // params.r_output_values = evaluator.execute();
}

}  // namespace blender::modifiers::geometry_nodes
