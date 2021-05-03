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
#include <tbb/task_group.h>

namespace blender::modifiers::geometry_nodes {

using bke::PersistentCollectionHandle;
using bke::PersistentObjectHandle;
using fn::CPPType;
using fn::GValueMap;
using nodes::GeoNodeExecParams;
using namespace fn::multi_function_types;

enum class ValueUsage {
  /* The input is definitely used by the node. */
  Yes,
  /* The input may be used by the node. */
  Maybe,
  /* The input will definitely not be used by the node. */
  No,
};

struct SingleInputValue {
  GMutablePointer value;
};

struct MultiInputValueItem {
  DSocket origin;
  GMutablePointer value;
};

struct MultiInputValue {
  /* Used when appending and checking if everything has been appended. */
  std::mutex mutex;

  Vector<MultiInputValueItem> values;
  int expected_size;
};

struct InputState {
  /**
   * How the node intends to use this input.
   */
  std::atomic<ValueUsage> usage = ValueUsage::Maybe;

  /**
   * Either points to SingleInputValue of MultiInputValue.
   */
  std::atomic<void *> value = nullptr;

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

  /** Only accessed while holding the schedule_mutex_. */
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
  NewNodeParamsProvider(NewGeometryNodesEvaluator &evaluator, DNode dnode) : evaluator_(evaluator)
  {
    this->dnode = dnode;
    this->handle_map = &evaluator.handle_map_;
    this->self_object = evaluator.self_object_;
    this->modifier = evaluator.modifier_;
    this->depsgraph = evaluator.depsgraph_;

    node_state_ = evaluator.node_states_.lookup(dnode);
  }

  bool can_get_input(StringRef identifier) const override
  {
    const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
    BLI_assert(socket);

    InputState &input_state = node_state_->inputs[socket->index()];
    if (!input_state.was_ready_for_evaluation.load(std::memory_order_acquire)) {
      return false;
    }
    return input_state.value.load(std::memory_order_acquire) != nullptr;
  }

  bool can_set_output(StringRef identifier) const override
  {
    const DOutputSocket socket = get_output_by_identifier(this->dnode, identifier);
    BLI_assert(socket);

    OutputState &output_state = node_state_->outputs[socket->index()];
    return !output_state.has_been_computed;
  }

  GMutablePointer extract_input(StringRef identifier) override
  {
    const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
    BLI_assert(socket);
    BLI_assert(!socket->is_multi_input_socket());

    InputState &input_state = node_state_->inputs[socket->index()];
    BLI_assert(input_state.was_ready_for_evaluation);
    SingleInputValue *value = (SingleInputValue *)input_state.value.load(
        std::memory_order_acquire);
    input_state.value.store(nullptr, std::memory_order_release);
    return value->value;
  }

  Vector<GMutablePointer> extract_multi_input(StringRef identifier) override
  {
    const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
    BLI_assert(socket);
    BLI_assert(socket->is_multi_input_socket());

    InputState &input_state = node_state_->inputs[socket->index()];
    BLI_assert(input_state.was_ready_for_evaluation);
    MultiInputValue *values = (MultiInputValue *)input_state.value.load(std::memory_order_acquire);
    input_state.value.store(nullptr, std::memory_order_release);

    Vector<GMutablePointer> ret_values;
    socket.foreach_origin_socket([&](DSocket origin) {
      for (const MultiInputValueItem &item : values->values) {
        if (item.origin == origin) {
          ret_values.append(item.value);
          return;
        }
      }
      BLI_assert_unreachable();
    });
    return ret_values;
  }

  GPointer get_input(StringRef identifier) const override
  {
    const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
    BLI_assert(socket);
    BLI_assert(!socket->is_multi_input_socket());

    InputState &input_state = node_state_->inputs[socket->index()];
    BLI_assert(input_state.was_ready_for_evaluation);
    SingleInputValue *value = (SingleInputValue *)input_state.value.load(
        std::memory_order_acquire);
    return value->value;
  }

  GMutablePointer alloc_output_value(const CPPType &type) override
  {
    return {type, evaluator_.allocator_.allocate(type.size(), type.alignment())};
  }

  void set_output(StringRef identifier, GMutablePointer value)
  {
    const DOutputSocket socket = get_output_by_identifier(this->dnode, identifier);
    BLI_assert(socket);

    OutputState &output_state = node_state_->outputs[socket->index()];
    BLI_assert(!output_state.has_been_computed);
    output_state.has_been_computed = true;
    evaluator_.forward_output(socket, value);
  }

  void require_input(StringRef identifier)
  {
    const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
    evaluator_.set_input_required(socket);
  }

  void set_input_unused(StringRef identifier)
  {
    const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
    evaluator_.set_input_unused(socket);
  }
};

class NewGeometryNodesEvaluator {
 private:
  /* TODO: Allocator per thread. */
  blender::LinearAllocator<> &allocator_;
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

  std::mutex schedule_mutex_;
  VectorSet<DNode> scheduled_nodes_;

  friend NewNodeParamsProvider;

 public:
  NewGeometryNodesEvaluator(GeometryNodesEvaluationParams &params)
      : allocator_(params.allocator),
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
      NodeState &node_state = *allocator_.construct<NodeState>().release();
      node_state.inputs.reinitialize(node->inputs().size());
      node_state.outputs.reinitialize(node->outputs().size());
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
    for (NodeState *node_state : node_states_.values()) {
      node_state->~NodeState();
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
    std::lock_guard lock{schedule_mutex_};
    if (node_state.is_running) {
      /* The node is currently running, it might have to run again because new inputs info is
       * available. */
      node_state.reschedule_after_run = true;
      return;
    }
    if (scheduled_nodes_.add(node)) {
      /* The node is not running and was not scheduled before. Just schedule it now. */
      task_group_.run([this]() { this->run_task(); });
      return;
    }
    /* The node is scheduled already and is not running. Therefore the latest info will be taken
     * into account when it runs next. No need to schedule it again. */
  }

  void run_task()
  {
    DNode node;
    NodeState *node_state;
    {
      std::lock_guard lock{schedule_mutex_};
      node = scheduled_nodes_.pop();
      node_state = node_states_.lookup(node);
      node_state->is_running = true;
    }

    if (node_state->runs == 0) {
      this->load_unlinked_inputs(node);
    }

    for (InputState &input_state : node_state->inputs) {
      /* TODO: Multi inputs. */
      if (input_state.value.load(std::memory_order_acquire) != nullptr) {
        input_state.was_ready_for_evaluation.store(true, std::memory_order_release);
      }
    }
    for (OutputState &output_state : node_state->outputs) {
      output_state.output_usage_for_evaluation = output_state.output_usage.load(
          std::memory_order_acquire);
    }

    NewNodeParamsProvider params_provider{*this, node};
    GeoNodeExecParams params{params_provider};
    node->typeinfo()->geometry_node_execute(params);

    node_state->runs++;

    {
      std::lock_guard lock{schedule_mutex_};
      node_state->is_running = false;

      if (node_state->reschedule_after_run) {
        scheduled_nodes_.add(node);
        node_state->reschedule_after_run = false;
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
        /* TODO */
      }
      else {
        if (origin_sockets.is_empty()) {
          GMutablePointer value = this->get_unlinked_input_value(input_socket, type);
          SingleInputValue *single_value = (SingleInputValue *)input_state.value.load(
              std::memory_order_acquire);
          single_value->value = value;
        }
        BLI_assert(origin_sockets.size() == 1);
        const DSocket origin = origin_sockets[0];
        if (origin->is_input()) {
          GMutablePointer value = this->get_unlinked_input_value(DInputSocket(origin), type);
          SingleInputValue *single_value = (SingleInputValue *)input_state.value.load(
              std::memory_order_acquire);
          single_value->value = value;
        }
      }
    }
  }

  GMutablePointer get_unlinked_input_value(const DInputSocket &socket,
                                           const CPPType &required_type)
  {
    bNodeSocket *bsocket = socket->bsocket();
    const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket->typeinfo());
    void *buffer = allocator_.allocate(type.size(), type.alignment());

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
      void *converted_buffer = allocator_.allocate(required_type.size(),
                                                   required_type.alignment());
      conversions_.convert_to_uninitialized(type, required_type, buffer, converted_buffer);
      type.destruct(buffer);
      return {required_type, converted_buffer};
    }
    void *default_buffer = allocator_.allocate(required_type.size(), required_type.alignment());
    required_type.copy_to_uninitialized(required_type.default_value(), default_buffer);
    return {required_type, default_buffer};
  }
};

class NodeParamsProvider : public nodes::GeoNodeExecParamsProvider {
 public:
  LinearAllocator<> *allocator;
  GValueMap<StringRef> *input_values;
  GValueMap<StringRef> *output_values;

  bool can_get_input(StringRef identifier) const override
  {
    return input_values->contains(identifier);
  }

  bool can_set_output(StringRef identifier) const override
  {
    return !output_values->contains(identifier);
  }

  GMutablePointer extract_input(StringRef identifier) override
  {
    return this->input_values->extract(identifier);
  }

  Vector<GMutablePointer> extract_multi_input(StringRef identifier) override
  {
    Vector<GMutablePointer> values;
    int index = 0;
    while (true) {
      std::string sub_identifier = identifier;
      if (index > 0) {
        sub_identifier += "[" + std::to_string(index) + "]";
      }
      if (!this->input_values->contains(sub_identifier)) {
        break;
      }
      values.append(input_values->extract(sub_identifier));
      index++;
    }
    return values;
  }

  GPointer get_input(StringRef identifier) const override
  {
    return this->input_values->lookup(identifier);
  }

  GMutablePointer alloc_output_value(StringRef identifier, const CPPType &type) override
  {
    void *buffer = this->allocator->allocate(type.size(), type.alignment());
    GMutablePointer ptr{&type, buffer};
    this->output_values->add_new_direct(identifier, ptr);
    return ptr;
  }
};

class GeometryNodesEvaluator {
 public:
  using LogSocketValueFn = std::function<void(DSocket, Span<GPointer>)>;

 private:
  blender::LinearAllocator<> &allocator_;
  Map<std::pair<DInputSocket, DOutputSocket>, GMutablePointer> value_by_input_;
  Vector<DInputSocket> group_outputs_;
  blender::nodes::MultiFunctionByNode &mf_by_node_;
  const blender::nodes::DataTypeConversions &conversions_;
  const PersistentDataHandleMap &handle_map_;
  const Object *self_object_;
  const ModifierData *modifier_;
  Depsgraph *depsgraph_;
  LogSocketValueFn log_socket_value_fn_;

 public:
  GeometryNodesEvaluator(GeometryNodesEvaluationParams &params)
      : allocator_(params.allocator),
        group_outputs_(std::move(params.output_sockets)),
        mf_by_node_(*params.mf_by_node),
        conversions_(blender::nodes::get_implicit_type_conversions()),
        handle_map_(*params.handle_map),
        self_object_(params.self_object),
        modifier_(&params.modifier_->modifier),
        depsgraph_(params.depsgraph),
        log_socket_value_fn_(std::move(params.log_socket_value_fn))
  {
    for (auto item : params.input_values.items()) {
      this->log_socket_value(item.key, item.value);
      this->forward_to_inputs(item.key, item.value);
    }
  }

  Vector<GMutablePointer> execute()
  {
    Vector<GMutablePointer> results;
    for (const DInputSocket &group_output : group_outputs_) {
      Vector<GMutablePointer> result = this->get_input_values(group_output);
      this->log_socket_value(group_output, result);
      results.append(result[0]);
    }
    for (GMutablePointer value : value_by_input_.values()) {
      value.destruct();
    }
    return results;
  }

 private:
  Vector<GMutablePointer> get_input_values(const DInputSocket socket_to_compute)
  {
    Vector<DSocket> from_sockets;
    socket_to_compute.foreach_origin_socket([&](DSocket socket) { from_sockets.append(socket); });

    if (from_sockets.is_empty()) {
      /* The input is not connected, use the value from the socket itself. */
      const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket_to_compute->typeinfo());
      return {get_unlinked_input_value(socket_to_compute, type)};
    }

    /* Multi-input sockets contain a vector of inputs. */
    if (socket_to_compute->is_multi_input_socket()) {
      return this->get_inputs_from_incoming_links(socket_to_compute, from_sockets);
    }

    const DSocket from_socket = from_sockets[0];
    GMutablePointer value = this->get_input_from_incoming_link(socket_to_compute, from_socket);
    return {value};
  }

  Vector<GMutablePointer> get_inputs_from_incoming_links(const DInputSocket socket_to_compute,
                                                         const Span<DSocket> from_sockets)
  {
    Vector<GMutablePointer> values;
    for (const int i : from_sockets.index_range()) {
      const DSocket from_socket = from_sockets[i];
      const int first_occurence = from_sockets.take_front(i).first_index_try(from_socket);
      if (first_occurence == -1) {
        values.append(this->get_input_from_incoming_link(socket_to_compute, from_socket));
      }
      else {
        /* If the same from-socket occurs more than once, we make a copy of the first value. This
         * can happen when a node linked to a multi-input-socket is muted. */
        GMutablePointer value = values[first_occurence];
        const CPPType *type = value.type();
        void *copy_buffer = allocator_.allocate(type->size(), type->alignment());
        type->copy_to_uninitialized(value.get(), copy_buffer);
        values.append({type, copy_buffer});
      }
    }
    return values;
  }

  GMutablePointer get_input_from_incoming_link(const DInputSocket socket_to_compute,
                                               const DSocket from_socket)
  {
    if (from_socket->is_output()) {
      const DOutputSocket from_output_socket{from_socket};
      const std::pair<DInputSocket, DOutputSocket> key = std::make_pair(socket_to_compute,
                                                                        from_output_socket);
      std::optional<GMutablePointer> value = value_by_input_.pop_try(key);
      if (value.has_value()) {
        /* This input has been computed before, return it directly. */
        return {*value};
      }

      /* Compute the socket now. */
      this->compute_output_and_forward(from_output_socket);
      return {value_by_input_.pop(key)};
    }

    /* Get value from an unlinked input socket. */
    const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket_to_compute->typeinfo());
    const DInputSocket from_input_socket{from_socket};
    return {get_unlinked_input_value(from_input_socket, type)};
  }

  void compute_output_and_forward(const DOutputSocket socket_to_compute)
  {
    const DNode node{socket_to_compute.context(), &socket_to_compute->node()};

    if (!socket_to_compute->is_available()) {
      /* If the output is not available, use a default value. */
      const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket_to_compute->typeinfo());
      void *buffer = allocator_.allocate(type.size(), type.alignment());
      type.copy_to_uninitialized(type.default_value(), buffer);
      this->forward_to_inputs(socket_to_compute, {type, buffer});
      return;
    }

    /* Prepare inputs required to execute the node. */
    GValueMap<StringRef> node_inputs_map{allocator_};
    for (const InputSocketRef *input_socket : node->inputs()) {
      if (input_socket->is_available()) {
        DInputSocket dsocket{node.context(), input_socket};
        Vector<GMutablePointer> values = this->get_input_values(dsocket);
        this->log_socket_value(dsocket, values);
        for (int i = 0; i < values.size(); ++i) {
          /* Values from Multi Input Sockets are stored in input map with the format
           * <identifier>[<index>]. */
          blender::StringRefNull key = allocator_.copy_string(
              input_socket->identifier() + (i > 0 ? ("[" + std::to_string(i)) + "]" : ""));
          node_inputs_map.add_new_direct(key, std::move(values[i]));
        }
      }
    }

    /* Execute the node. */
    GValueMap<StringRef> node_outputs_map{allocator_};
    NodeParamsProvider params_provider;
    params_provider.dnode = node;
    params_provider.handle_map = &handle_map_;
    params_provider.self_object = self_object_;
    params_provider.depsgraph = depsgraph_;
    params_provider.allocator = &allocator_;
    params_provider.input_values = &node_inputs_map;
    params_provider.output_values = &node_outputs_map;
    params_provider.modifier = modifier_;
    this->execute_node(node, params_provider);

    /* Forward computed outputs to linked input sockets. */
    for (const OutputSocketRef *output_socket : node->outputs()) {
      if (output_socket->is_available()) {
        const DOutputSocket dsocket{node.context(), output_socket};
        GMutablePointer value = node_outputs_map.extract(output_socket->identifier());
        this->log_socket_value(dsocket, value);
        this->forward_to_inputs(dsocket, value);
      }
    }
  }

  void log_socket_value(const DSocket socket, Span<GPointer> values)
  {
    if (log_socket_value_fn_) {
      log_socket_value_fn_(socket, values);
    }
  }

  void log_socket_value(const DSocket socket, Span<GMutablePointer> values)
  {
    this->log_socket_value(socket, values.cast<GPointer>());
  }

  void log_socket_value(const DSocket socket, GPointer value)
  {
    this->log_socket_value(socket, Span<GPointer>(&value, 1));
  }

  void execute_node(const DNode node, NodeParamsProvider &params_provider)
  {
    const bNode &bnode = *params_provider.dnode->bnode();

    /* Use the geometry-node-execute callback if it exists. */
    if (bnode.typeinfo->geometry_node_execute != nullptr) {
      GeoNodeExecParams params{params_provider};
      bnode.typeinfo->geometry_node_execute(params);
      return;
    }

    /* Use the multi-function implementation if it exists. */
    const MultiFunction *multi_function = mf_by_node_.lookup_default(node, nullptr);
    if (multi_function != nullptr) {
      this->execute_multi_function_node(node, params_provider, *multi_function);
      return;
    }

    /* Just output default values if no implementation exists. */
    this->execute_unknown_node(node, params_provider);
  }

  void execute_multi_function_node(const DNode node,
                                   NodeParamsProvider &params_provider,
                                   const MultiFunction &fn)
  {
    MFContextBuilder fn_context;
    MFParamsBuilder fn_params{fn, 1};
    Vector<GMutablePointer> input_data;
    for (const InputSocketRef *socket_ref : node->inputs()) {
      if (socket_ref->is_available()) {
        GMutablePointer data = params_provider.extract_input(socket_ref->identifier());
        fn_params.add_readonly_single_input(GSpan(*data.type(), data.get(), 1));
        input_data.append(data);
      }
    }
    Vector<GMutablePointer> output_data;
    for (const OutputSocketRef *socket_ref : node->outputs()) {
      if (socket_ref->is_available()) {
        const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket_ref->typeinfo());
        GMutablePointer output_value = params_provider.alloc_output_value(socket_ref->identifier(),
                                                                          type);
        fn_params.add_uninitialized_single_output(GMutableSpan{type, output_value.get(), 1});
        output_data.append(output_value);
      }
    }
    fn.call(IndexRange(1), fn_params, fn_context);
    for (GMutablePointer value : input_data) {
      value.destruct();
    }
  }

  void execute_unknown_node(const DNode node, NodeParamsProvider &params_provider)
  {
    for (const OutputSocketRef *socket : node->outputs()) {
      if (socket->is_available()) {
        const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket->typeinfo());
        params_provider.output_values->add_new_by_copy(socket->identifier(),
                                                       {type, type.default_value()});
      }
    }
  }

  void forward_to_inputs(const DOutputSocket from_socket, GMutablePointer value_to_forward)
  {
    /* For all sockets that are linked with the from_socket push the value to their node. */
    Vector<DInputSocket> to_sockets_all;

    auto handle_target_socket_fn = [&](DInputSocket to_socket) {
      to_sockets_all.append_non_duplicates(to_socket);
    };
    auto handle_skipped_socket_fn = [&, this](DSocket socket) {
      this->log_socket_value(socket, value_to_forward);
    };

    from_socket.foreach_target_socket(handle_target_socket_fn, handle_skipped_socket_fn);

    const CPPType &from_type = *value_to_forward.type();
    Vector<DInputSocket> to_sockets_same_type;
    for (const DInputSocket &to_socket : to_sockets_all) {
      const CPPType &to_type = *blender::nodes::socket_cpp_type_get(*to_socket->typeinfo());
      const std::pair<DInputSocket, DOutputSocket> key = std::make_pair(to_socket, from_socket);
      if (from_type == to_type) {
        to_sockets_same_type.append(to_socket);
      }
      else {
        void *buffer = allocator_.allocate(to_type.size(), to_type.alignment());
        if (conversions_.is_convertible(from_type, to_type)) {
          conversions_.convert_to_uninitialized(
              from_type, to_type, value_to_forward.get(), buffer);
        }
        else {
          to_type.copy_to_uninitialized(to_type.default_value(), buffer);
        }
        add_value_to_input_socket(key, GMutablePointer{to_type, buffer});
      }
    }

    if (to_sockets_same_type.size() == 0) {
      /* This value is not further used, so destruct it. */
      value_to_forward.destruct();
    }
    else if (to_sockets_same_type.size() == 1) {
      /* This value is only used on one input socket, no need to copy it. */
      const DInputSocket to_socket = to_sockets_same_type[0];
      const std::pair<DInputSocket, DOutputSocket> key = std::make_pair(to_socket, from_socket);

      add_value_to_input_socket(key, value_to_forward);
    }
    else {
      /* Multiple inputs use the value, make a copy for every input except for one. */
      const DInputSocket first_to_socket = to_sockets_same_type[0];
      Span<DInputSocket> other_to_sockets = to_sockets_same_type.as_span().drop_front(1);
      const CPPType &type = *value_to_forward.type();
      const std::pair<DInputSocket, DOutputSocket> first_key = std::make_pair(first_to_socket,
                                                                              from_socket);
      add_value_to_input_socket(first_key, value_to_forward);
      for (const DInputSocket &to_socket : other_to_sockets) {
        const std::pair<DInputSocket, DOutputSocket> key = std::make_pair(to_socket, from_socket);
        void *buffer = allocator_.allocate(type.size(), type.alignment());
        type.copy_to_uninitialized(value_to_forward.get(), buffer);
        add_value_to_input_socket(key, GMutablePointer{type, buffer});
      }
    }
  }

  void add_value_to_input_socket(const std::pair<DInputSocket, DOutputSocket> key,
                                 GMutablePointer value)
  {
    value_by_input_.add_new(key, value);
  }

  GMutablePointer get_unlinked_input_value(const DInputSocket &socket,
                                           const CPPType &required_type)
  {
    bNodeSocket *bsocket = socket->bsocket();
    const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket->typeinfo());
    void *buffer = allocator_.allocate(type.size(), type.alignment());

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
      void *converted_buffer = allocator_.allocate(required_type.size(),
                                                   required_type.alignment());
      conversions_.convert_to_uninitialized(type, required_type, buffer, converted_buffer);
      type.destruct(buffer);
      return {required_type, converted_buffer};
    }
    void *default_buffer = allocator_.allocate(required_type.size(), required_type.alignment());
    required_type.copy_to_uninitialized(required_type.default_value(), default_buffer);
    return {required_type, default_buffer};
  }
};

void evaluate_geometry_nodes(GeometryNodesEvaluationParams &params)
{
  GeometryNodesEvaluator evaluator{params};
  params.r_output_values = evaluator.execute();
}

}  // namespace blender::modifiers::geometry_nodes
