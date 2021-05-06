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

enum class ValueUsage : uint8_t {
  /* The value is definitely used. */
  Yes,
  /* The value may be used. */
  Maybe,
  /* The value will definitely not be used. */
  No,
};

struct SingleInputValue {
  void *value = nullptr;
};

struct MultiInputValueItem {
  DSocket origin;
  void *value = nullptr;
};

struct MultiInputValue {
  Vector<MultiInputValueItem> items;
  int expected_size;
};

struct InputState {
  /**
   * How the node intends to use this input.
   */
  ValueUsage usage = ValueUsage::Maybe;

  /**
   * Type of the socket. If this is null, the socket should just be ignored.
   */
  const CPPType *type = nullptr;

  /**
   * Value of this input socket. By default, the value is empty. When other nodes are done
   * computing their outputs, the computed values will be forwarded to linked input sockets.
   * The value will then live here until it is consumed by the node or it was found that the value
   * is not needed anymore.
   */
  union {
    SingleInputValue *single;
    MultiInputValue *multi;
  } value;

  /**
   * True when this input is/was used for an evaluation. While a node is running, only the inputs
   * that have this set to true are allowed to be used. This makes sure that inputs created while
   * the node is running correctly trigger the node to run again.
   *
   * While the node is running, this can be checked without a lock, because no one is writing to
   * it. If this is true, the value can be read without a lock as well, because the value is not
   * changed by others anymore.
   */
  bool was_ready_for_evaluation = false;
};

struct OutputState {
  /**
   * If this output has been computed and forwarded already.
   */
  bool has_been_computed = false;

  /**
   * Anyone can update this value (after locking the node mutex) to tell the node what outputs are
   * (not) required.
   */
  ValueUsage output_usage = ValueUsage::Maybe;

  /**
   * This is a copy of `output_usage` that is done right before node evaluation starts. This is
   * done so that the node gets a consistent view of what outputs are used, even when this changes
   * while the node is running (the node might be reevaluated in that case).
   *
   * While the node is running, this can be checked without a lock, because no one is writing to
   * it.
   */
  ValueUsage output_usage_for_evaluation;
};

enum class NodeScheduleState {
  NotScheduled,
  Scheduled,
  Running,
  RunningAndRescheduled,
};

struct NodeState {
  /**
   * Needs to be locked when any data in this state is accessed that is not explicitely marked as
   * otherwise.
   */
  std::mutex mutex;

  /**
   * States of the individual input and output sockets. One can index into these arrays without
   * locking.
   */
  Array<InputState> inputs;
  Array<OutputState> outputs;

  /**
   * The first run of a node is sometimes handled specially.
   */
  bool is_first_run = true;

  /**
   * A node is always in one specific schedule state. This helps to ensure that the same node does
   * not run twice at the same time accidentally.
   */
  NodeScheduleState schedule_state = NodeScheduleState::NotScheduled;
};

class NodeStateLock {
 private:
  std::lock_guard<std::mutex> lock_;
  const DNode node_;

 public:
  NodeStateLock(const DNode node, NodeState &node_state) : lock_(node_state.mutex), node_(node)
  {
  }

  /* Asserts that this node is locked. */
  void assert_is_node(const DNode node) const
  {
    if (node_ != node) {
      BLI_assert_unreachable();
    }
  }
};

class GeometryNodesEvaluator;

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

class NodeParamsProvider : public nodes::GeoNodeExecParamsProvider {
 private:
  GeometryNodesEvaluator &evaluator_;
  NodeState *node_state_;

 public:
  NodeParamsProvider(GeometryNodesEvaluator &evaluator, DNode dnode);

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

class GeometryNodesEvaluator {
 private:
  LinearAllocator<> &main_allocator_;
  tbb::enumerable_thread_specific<LinearAllocator<>> local_allocators_;
  Vector<DInputSocket> group_outputs_;
  Map<DOutputSocket, GMutablePointer> &input_values_;
  blender::nodes::MultiFunctionByNode &mf_by_node_;
  const blender::nodes::DataTypeConversions &conversions_;
  const PersistentDataHandleMap &handle_map_;
  const Object *self_object_;
  const ModifierData *modifier_;
  Depsgraph *depsgraph_;
  LogSocketValueFn log_socket_value_fn_;

  Map<DNode, NodeState *> node_states_;
  tbb::task_group task_group_;

  friend NodeParamsProvider;

 public:
  GeometryNodesEvaluator(GeometryNodesEvaluationParams &params)
      : main_allocator_(params.allocator),
        group_outputs_(std::move(params.output_sockets)),
        input_values_(params.input_values),
        mf_by_node_(*params.mf_by_node),
        conversions_(blender::nodes::get_implicit_type_conversions()),
        handle_map_(*params.handle_map),
        self_object_(params.self_object),
        modifier_(&params.modifier_->modifier),
        depsgraph_(params.depsgraph),
        log_socket_value_fn_(std::move(params.log_socket_value_fn))
  {
  }

  Vector<GMutablePointer> execute()
  {
    this->create_states_for_reachable_nodes();
    this->forward_input_values();
    this->schedule_initial_nodes();
    task_group_.wait();
    Vector<GMutablePointer> output_values = this->extract_output_values();
    this->free_states();
    return output_values;
  }

  Vector<GMutablePointer> extract_output_values()
  {
    Vector<GMutablePointer> output_values;
    for (const DInputSocket &socket : group_outputs_) {
      BLI_assert(socket->is_available());
      BLI_assert(!socket->is_multi_input_socket());

      const DNode node = socket.node();
      NodeState &node_state = *node_states_.lookup(node);
      if (node_state.is_first_run) {
        NodeStateLock node_lock(node, node_state);
        this->load_unlinked_inputs(node, node_state, node_lock);
        node_state.is_first_run = false;
      }
      InputState &input_state = node_state.inputs[socket->index()];
      const CPPType &type = *input_state.type;
      SingleInputValue &single_value = *input_state.value.single;
      void *value = single_value.value;
      BLI_assert(value != nullptr);

      /* Move value into memory owned by the main allocator. */
      void *buffer = main_allocator_.allocate(type.size(), type.alignment());
      type.move_to_uninitialized(value, buffer);

      output_values.append({type, buffer});
    }
    return output_values;
  }

  void forward_input_values()
  {
    ForwardSettings settings;
    settings.is_forwarding_group_inputs = true;

    for (auto &&item : input_values_.items()) {
      const DOutputSocket socket = item.key;
      GMutablePointer value = item.value;

      const DNode node = socket.node();
      NodeState *node_state = node_states_.lookup_default(node, nullptr);
      if (node_state == nullptr) {
        /* The socket is not connected to any output. */
        value.destruct();
        continue;
      }
      this->forward_output(socket, value, settings);
    }
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
        const CPPType *type = this->get_socket_type(socket_ref);
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
        if (input_state.type != nullptr) {
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
            void *value = single_value.value;
            if (value != nullptr) {
              input_state.type->destruct(value);
            }
            single_value.~SingleInputValue();
          }
        }
      }

      node_state.~NodeState();
    }
  }

  void schedule_initial_nodes()
  {
    for (const DInputSocket &socket : group_outputs_) {
      const DNode node = socket.node();
      NodeState &node_state = *node_states_.lookup(socket.node());
      NodeStateLock node_lock{node, node_state};
      this->set_input_required(socket, node_state, node_lock);
    }
  }

  void set_input_required(const DInputSocket socket,
                          NodeState &node_state,
                          const NodeStateLock &node_lock)
  {
    const DNode node = socket.node();
    InputState &input_state = node_state.inputs[socket->index()];

    /* Value set as unused cannot become used again. */
    BLI_assert(input_state.usage != ValueUsage::No);

    if (input_state.was_ready_for_evaluation) {
      /* The value was already ready, but the node might expect to be evaluated again. */
      this->schedule_node_if_necessary(node, node_state, node_lock);
      return;
    }

    const ValueUsage old_usage = input_state.usage;
    if (old_usage == ValueUsage::Yes) {
      /* The node is already required, but the node might expect to be evaluated again. */
      this->schedule_node_if_necessary(node, node_state, node_lock);
      return;
    }

    /* Set usage of input correctly. */
    input_state.usage = ValueUsage::Yes;

    socket.foreach_origin_socket([&, this](const DSocket origin_socket) {
      if (origin_socket->is_input()) {
        /* These sockets are handled separately. */
        return;
      }
      const DNode origin_node = origin_socket.node();
      NodeState &origin_node_state = *this->node_states_.lookup(origin_node);
      OutputState &origin_socket_state = origin_node_state.outputs[origin_socket->index()];

      NodeStateLock origin_lock{origin_node, origin_node_state};

      if (origin_socket_state.output_usage == ValueUsage::Yes) {
        /* Output is marked as required already. So the other node is scheduled already. */
        return;
      }

      /* The origin node needs to be scheduled so that it provides the requested input
       * eventually. */
      origin_socket_state.output_usage = ValueUsage::Yes;
      this->schedule_node_if_necessary(origin_node, origin_node_state, origin_lock);
    });
  }

  void set_input_unused(DInputSocket UNUSED(socket), const NodeStateLock &UNUSED(node_lock))
  {
    /* TODO: Implementing this is an optimization. */
  }

  struct ForwardSettings {
    bool is_forwarding_group_inputs = false;
  };

  void forward_output(const DOutputSocket from_socket,
                      GMutablePointer value_to_forward,
                      const ForwardSettings &settings)
  {
    BLI_assert(value_to_forward.get() != nullptr);

    Vector<DInputSocket> to_sockets;

    auto handle_target_socket_fn = [&, this](const DInputSocket to_socket) {
      if (this->should_forward_to_socket(to_socket)) {
        to_sockets.append(to_socket);
      }
    };

    auto handle_skipped_socket_fn = [&](DSocket UNUSED(socket)) {};

    from_socket.foreach_target_socket(handle_target_socket_fn, handle_skipped_socket_fn);

    LinearAllocator<> &allocator = local_allocators_.local();

    const CPPType &from_type = *value_to_forward.type();
    Vector<DInputSocket> to_sockets_same_type;
    for (const DInputSocket &to_socket : to_sockets) {
      const CPPType &to_type = *this->get_socket_type(to_socket);
      if (from_type == to_type) {
        to_sockets_same_type.append(to_socket);
        continue;
      }
      this->forward_to_socket_with_different_type(
          allocator, value_to_forward, from_socket, to_socket, to_type, settings);
    }
    this->forward_to_sockets_with_same_type(
        allocator, to_sockets_same_type, value_to_forward, from_socket, settings);
  }

  bool should_forward_to_socket(const DInputSocket socket)
  {
    if (!socket->is_available()) {
      /* Unavailable sockets are never used. */
      return false;
    }
    const DNode to_node = socket.node();
    NodeState *target_node_state = this->node_states_.lookup_default(to_node, nullptr);
    if (target_node_state == nullptr) {
      /* If the socket belongs to a node that has no state, the entire node is not used. */
      return false;
    }
    InputState &target_input_state = target_node_state->inputs[socket->index()];

    std::lock_guard lock{target_node_state->mutex};
    return target_input_state.usage != ValueUsage::No;
  }

  void forward_to_socket_with_different_type(LinearAllocator<> &allocator,
                                             const GPointer value_to_forward,
                                             const DOutputSocket from_socket,
                                             const DInputSocket to_socket,
                                             const CPPType &to_type,
                                             const ForwardSettings &settings)
  {
    const CPPType &from_type = *value_to_forward.type();
    void *buffer = allocator.allocate(to_type.size(), to_type.alignment());
    if (conversions_.is_convertible(from_type, to_type)) {
      conversions_.convert_to_uninitialized(from_type, to_type, value_to_forward.get(), buffer);
    }
    else {
      /* Cannot convert, use default value instead. */
      to_type.copy_to_uninitialized(to_type.default_value(), buffer);
    }
    this->add_value_to_input_socket(to_socket, from_socket, {to_type, buffer}, settings);
  }

  void forward_to_sockets_with_same_type(LinearAllocator<> &allocator,
                                         Span<DInputSocket> to_sockets,
                                         GMutablePointer value_to_forward,
                                         const DOutputSocket from_socket,
                                         const ForwardSettings &settings)
  {
    if (to_sockets.is_empty()) {
      /* Value is not used anymore, so it can be destructed. */
      value_to_forward.destruct();
    }
    else if (to_sockets.size() == 1) {
      /* Value is only used by one input socket, no need to copy it. */
      const DInputSocket to_socket = to_sockets[0];
      this->add_value_to_input_socket(to_socket, from_socket, value_to_forward, settings);
    }
    else {
      /* Multiple inputs use the value, make a copy for every input except for one. */
      /* First make the copies, so that the next node does not start modifying the value while we
       * are still making copies. */
      const CPPType &type = *value_to_forward.type();
      for (const DInputSocket &to_socket : to_sockets.drop_front(1)) {
        void *buffer = allocator.allocate(type.size(), type.alignment());
        type.copy_to_uninitialized(value_to_forward.get(), buffer);
        this->add_value_to_input_socket(to_socket, from_socket, {type, buffer}, settings);
      }
      /* Forward the original value to one of the targets. */
      const DInputSocket to_socket = to_sockets[0];
      this->add_value_to_input_socket(to_socket, from_socket, value_to_forward, settings);
    }
  }

  void add_value_to_input_socket(const DInputSocket socket,
                                 const DOutputSocket origin,
                                 GMutablePointer value,
                                 const ForwardSettings &settings)
  {
    BLI_assert(socket->is_available());

    const DNode node = socket.node();
    NodeState &node_state = *node_states_.lookup(node);
    InputState &input_state = node_state.inputs[socket->index()];

    NodeStateLock node_lock{node, node_state};

    if (socket->is_multi_input_socket()) {
      MultiInputValue &multi_value = *input_state.value.multi;
      multi_value.items.append({origin, value.get()});
    }
    else {
      SingleInputValue &single_value = *input_state.value.single;
      BLI_assert(single_value.value == nullptr);
      single_value.value = value.get();
    }
    /* We don't want to trigger nodes that might not be needed after all. */
    if (!settings.is_forwarding_group_inputs) {
      /* TODO: Schedule in fewer cases. */
      this->schedule_node_if_necessary(node, node_state, node_lock);
    }
  }

  const CPPType *get_socket_type(const DSocket socket) const
  {
    return nodes::socket_cpp_type_get(*socket->typeinfo());
  }

  const CPPType *get_socket_type(const SocketRef &socket) const
  {
    return nodes::socket_cpp_type_get(*socket.typeinfo());
  }

  void schedule_node_if_necessary(const DNode node,
                                  NodeState &node_state,
                                  const NodeStateLock &node_lock)
  {
    node_lock.assert_is_node(node);
    switch (node_state.schedule_state) {
      case NodeScheduleState::NotScheduled: {
        /* Schedule the node now. */
        node_state.schedule_state = NodeScheduleState::Scheduled;
        this->add_node_to_task_group(node);
        break;
      }
      case NodeScheduleState::Scheduled: {
        /* Scheduled already, nothing to do. */
        break;
      }
      case NodeScheduleState::Running: {
        /* Reschedule node while it is running. The node will reschedule itself when it is done. */
        node_state.schedule_state = NodeScheduleState::RunningAndRescheduled;
        break;
      }
      case NodeScheduleState::RunningAndRescheduled: {
        /* Scheduled already, nothing to do. */
        break;
      }
    }
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
      BLI_assert(node_state.schedule_state == NodeScheduleState::Scheduled);
      node_state.schedule_state = NodeScheduleState::Running;
    }

    this->run_node(node, node_state);

    {
      std::lock_guard lock{node_state.mutex};
      if (node_state.schedule_state == NodeScheduleState::Running) {
        node_state.schedule_state = NodeScheduleState::NotScheduled;
      }
      else if (node_state.schedule_state == NodeScheduleState::RunningAndRescheduled) {
        this->add_node_to_task_group(node);
        node_state.schedule_state = NodeScheduleState::Scheduled;
      }
      else {
        BLI_assert_unreachable();
      }
    }
  }

  void run_node(const DNode node, NodeState &node_state)
  {
    if (node_state.is_first_run) {
      this->first_node_run(node, node_state);
      node_state.is_first_run = false;
    }

    bool all_required_inputs_available = true;
    {
      NodeStateLock node_lock{node, node_state};
      for (const int i : node_state.inputs.index_range()) {
        InputState &input_state = node_state.inputs[i];
        if (input_state.type == nullptr) {
          continue;
        }
        const InputSocketRef &socket_ref = node->input(i);
        const bool is_required = input_state.usage == ValueUsage::Yes;

        /* No need to check this socket again. */
        if (input_state.was_ready_for_evaluation) {
          continue;
        }

        if (socket_ref.is_multi_input_socket()) {
          MultiInputValue &multi_value = *input_state.value.multi;
          if (multi_value.items.size() == multi_value.expected_size) {
            input_state.was_ready_for_evaluation = true;
          }
          else if (is_required) {
            all_required_inputs_available = false;
          }
        }
        else {
          SingleInputValue &single_value = *input_state.value.single;
          if (single_value.value != nullptr) {
            input_state.was_ready_for_evaluation = true;
          }
          else if (is_required) {
            all_required_inputs_available = false;
          }
        }
      }
    }
    if (!all_required_inputs_available) {
      return;
    }
    bool evaluation_is_necessary = false;
    {
      NodeStateLock node_lock{node, node_state};
      for (OutputState &output_state : node_state.outputs) {
        output_state.output_usage_for_evaluation = output_state.output_usage;
        if (output_state.output_usage_for_evaluation == ValueUsage::Yes) {
          if (!output_state.has_been_computed) {
            /* Only evaluate when there is an output that is required but has not been computed. */
            evaluation_is_necessary = true;
          }
        }
      }
    }
    if (!evaluation_is_necessary) {
      return;
    }
    this->execute_node(node, node_state);
  }

  void first_node_run(const DNode node, NodeState &node_state)
  {
    NodeStateLock node_lock{node, node_state};

    this->load_unlinked_inputs(node, node_state, node_lock);

    /* Set all the sockets as required that are always required. */
    Vector<int> required_inputs;
    this->get_always_required_input_indices(node, required_inputs);
    for (const int i : required_inputs) {
      const DInputSocket socket = node.input(i);
      this->set_input_required(socket, node_state, node_lock);
    }
  }

  void get_always_required_input_indices(const DNode node, Vector<int> &indices)
  {
    /* Temporary solution: just set all inputs as required in the first run. */
    for (const InputSocketRef *socket_ref : node->inputs()) {
      if (!socket_ref->is_available()) {
        continue;
      }
      const CPPType *type = this->get_socket_type(*socket_ref);
      if (type == nullptr) {
        continue;
      }
      indices.append(socket_ref->index());
    }
  }

  void execute_node(const DNode node, NodeState &node_state)
  {
    if (node->is_group_input_node()) {
      return;
    }

    const bNode &bnode = *node->bnode();

    /* Use the geometry node execute callback if it exists. */
    if (bnode.typeinfo->geometry_node_execute != nullptr) {
      this->execute_geometry_node(node);
      return;
    }

    /* Use the multi-function implementation if it exists. */
    const MultiFunction *multi_function = mf_by_node_.lookup_default(node, nullptr);
    if (multi_function != nullptr) {
      this->execute_multi_function_node(node, *multi_function, node_state);
      return;
    }

    this->execute_unknown_node(node);
  }

  void execute_geometry_node(const DNode node)
  {
    const bNode &bnode = *node->bnode();

    NodeParamsProvider params_provider{*this, node};
    GeoNodeExecParams params{params_provider};
    bnode.typeinfo->geometry_node_execute(params);
  }

  void execute_multi_function_node(const DNode node,
                                   const MultiFunction &fn,
                                   NodeState &node_state)
  {
    MFContextBuilder fn_context;
    MFParamsBuilder fn_params{fn, 1};
    LinearAllocator<> &allocator = local_allocators_.local();
    for (const int i : node->inputs().index_range()) {
      const InputSocketRef &socket_ref = node->input(i);
      if (!socket_ref.is_available()) {
        continue;
      }
      BLI_assert(!socket_ref.is_multi_input_socket());
      InputState &input_state = node_state.inputs[i];
      BLI_assert(input_state.was_ready_for_evaluation);
      SingleInputValue &single_value = *input_state.value.single;
      BLI_assert(single_value.value != nullptr);
      fn_params.add_readonly_single_input(GPointer{*input_state.type, single_value.value});
    }
    Vector<GMutablePointer> outputs;
    for (const int i : node->outputs().index_range()) {
      const OutputSocketRef &socket_ref = node->output(i);
      if (!socket_ref.is_available()) {
        continue;
      }
      const CPPType &type = *this->get_socket_type(socket_ref);
      void *buffer = allocator.allocate(type.size(), type.alignment());
      fn_params.add_uninitialized_single_output(GMutableSpan{type, buffer, 1});
      outputs.append({type, buffer});
    }

    fn.call(IndexRange(1), fn_params, fn_context);

    int output_index = 0;
    for (const int i : node->outputs().index_range()) {
      const OutputSocketRef &socket_ref = node->output(i);
      if (!socket_ref.is_available()) {
        continue;
      }
      OutputState &output_state = node_state.outputs[i];
      const DOutputSocket socket{node.context(), &socket_ref};
      GMutablePointer value = outputs[output_index];
      this->forward_output(socket, value, {});
      output_state.has_been_computed = true;
      output_index++;
    }
  }

  void execute_unknown_node(const DNode node)
  {
    NodeState &node_state = *node_states_.lookup(node);
    LinearAllocator<> &allocator = local_allocators_.local();
    for (const OutputSocketRef *socket : node->outputs()) {
      if (!socket->is_available()) {
        continue;
      }
      const CPPType *type = this->get_socket_type(*socket);
      if (type == nullptr) {
        continue;
      }
      OutputState &output_state = node_state.outputs[socket->index()];
      output_state.has_been_computed = true;
      void *buffer = allocator.allocate(type->size(), type->alignment());
      type->copy_to_uninitialized(type->default_value(), buffer);
      this->forward_output({node.context(), socket}, {*type, buffer}, {});
    }
  }

  void load_unlinked_inputs(const DNode node,
                            NodeState &node_state,
                            const NodeStateLock &node_lock)
  {
    node_lock.assert_is_node(node);
    for (const InputSocketRef *input_socket_ref : node->inputs()) {
      if (!input_socket_ref->is_available()) {
        continue;
      }
      InputState &input_state = node_state.inputs[input_socket_ref->index()];
      if (input_state.type == nullptr) {
        continue;
      }
      const DInputSocket input_socket{node.context(), input_socket_ref};
      const CPPType &type = *input_state.type;

      Vector<DSocket> origin_sockets;
      input_socket.foreach_origin_socket([&](DSocket origin) { origin_sockets.append(origin); });

      if (input_socket->is_multi_input_socket()) {
        MultiInputValue &multi_value = *input_state.value.multi;
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
          single_value.value = value.get();
        }
        else {
          BLI_assert(origin_sockets.size() == 1);
          const DSocket origin = origin_sockets[0];
          if (origin->is_input()) {
            GMutablePointer value = this->get_unlinked_input_value(DInputSocket(origin), type);
            single_value.value = value.get();
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
    const CPPType &type = *this->get_socket_type(socket);
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

NodeParamsProvider::NodeParamsProvider(GeometryNodesEvaluator &evaluator, DNode dnode)
    : evaluator_(evaluator)
{
  this->dnode = dnode;
  this->handle_map = &evaluator.handle_map_;
  this->self_object = evaluator.self_object_;
  this->modifier = evaluator.modifier_;
  this->depsgraph = evaluator.depsgraph_;

  node_state_ = evaluator.node_states_.lookup(dnode);
}

bool NodeParamsProvider::can_get_input(StringRef identifier) const
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);

  InputState &input_state = node_state_->inputs[socket->index()];
  if (!input_state.was_ready_for_evaluation) {
    return false;
  }

  if (socket->is_multi_input_socket()) {
    MultiInputValue &multi_value = *input_state.value.multi;
    return multi_value.items.size() == multi_value.expected_size;
  }
  SingleInputValue &single_value = *input_state.value.single;
  return single_value.value != nullptr;
}

bool NodeParamsProvider::can_set_output(StringRef identifier) const
{
  const DOutputSocket socket = get_output_by_identifier(this->dnode, identifier);
  BLI_assert(socket);

  OutputState &output_state = node_state_->outputs[socket->index()];
  return !output_state.has_been_computed;
}

GMutablePointer NodeParamsProvider::extract_input(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);
  BLI_assert(!socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_->inputs[socket->index()];
  SingleInputValue &single_value = *input_state.value.single;
  void *value = single_value.value;
  single_value.value = nullptr;
  return {*input_state.type, value};
}

Vector<GMutablePointer> NodeParamsProvider::extract_multi_input(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);
  BLI_assert(socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_->inputs[socket->index()];
  MultiInputValue &multi_value = *input_state.value.multi;

  Vector<GMutablePointer> ret_values;
  socket.foreach_origin_socket([&](DSocket origin) {
    for (const MultiInputValueItem &item : multi_value.items) {
      if (item.origin == origin) {
        ret_values.append({*input_state.type, item.value});
        return;
      }
    }
    BLI_assert_unreachable();
  });
  multi_value.items.clear();
  return ret_values;
}

GPointer NodeParamsProvider::get_input(StringRef identifier) const
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);
  BLI_assert(!socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_->inputs[socket->index()];
  SingleInputValue &single_value = *input_state.value.single;
  return {*input_state.type, single_value.value};
}

GMutablePointer NodeParamsProvider::alloc_output_value(const CPPType &type)
{
  LinearAllocator<> &allocator = evaluator_.local_allocators_.local();
  return {type, allocator.allocate(type.size(), type.alignment())};
}

void NodeParamsProvider::set_output(StringRef identifier, GMutablePointer value)
{
  const DOutputSocket socket = get_output_by_identifier(this->dnode, identifier);
  BLI_assert(socket);

  OutputState &output_state = node_state_->outputs[socket->index()];
  BLI_assert(!output_state.has_been_computed);
  evaluator_.forward_output(socket, value, {});
  output_state.has_been_computed = true;
}

void NodeParamsProvider::require_input(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  NodeStateLock node_lock(this->dnode, *node_state_);
  evaluator_.set_input_required(socket, *node_state_, node_lock);
}

void NodeParamsProvider::set_input_unused(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  NodeStateLock node_lock(this->dnode, *node_state_);
  evaluator_.set_input_unused(socket, node_lock);
}

void evaluate_geometry_nodes(GeometryNodesEvaluationParams &params)
{
  GeometryNodesEvaluator evaluator{params};
  params.r_output_values = evaluator.execute();
}

}  // namespace blender::modifiers::geometry_nodes
