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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup fn
 */

#include <mutex>

#include "BLI_cpp_type.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_function_ref.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_stack.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

#include "FN_sgraph.hh"

namespace blender::fn::sgraph {

enum class LazyRequireInputResult {
  Ready,
  NotYetAvailable,
};

class ExecuteNodeParams {
 public:
  virtual bool is_input_available(int index) const = 0;
  virtual bool output_was_set(int index) const = 0;

  virtual GMutablePointer extract_single_input(int index) = 0;
  virtual GPointer get_input(int index) const = 0;

  virtual void set_output_by_copy(int index, GPointer value) = 0;
  virtual void set_output_by_move(int index, GMutablePointer value) = 0;

  virtual bool output_maybe_required(int index) const = 0;

  virtual LazyRequireInputResult set_input_required(int index) = 0;
  virtual void set_input_unused(int index) = 0;
  virtual bool output_is_required(int index) = 0;
};

class ExecuteGraphIO {
 public:
  virtual LazyRequireInputResult require_input(int index) = 0;
  virtual void load_input_to_uninitialized(int index, GMutablePointer r_value) = 0;
  virtual bool can_load_input(int index) const = 0;
  virtual bool output_is_required(int index) const = 0;
  virtual void set_output_by_copy(int index, GPointer value) = 0;
};

template<typename SGraphAdapter> class SGraphExecuteSemantics {
 public:
  using NodeID = typename SGraphAdapter::NodeID;

  virtual const CPPType *input_socket_type(const NodeID &node, int input_index) const = 0;
  virtual const CPPType *output_socket_type(const NodeID &node, int output_index) const = 0;
  virtual void load_unlinked_single_input(const NodeID &node,
                                          int input_index,
                                          GMutablePointer r_value) const = 0;
  virtual bool is_multi_input(const NodeID &node, const int input_index) const = 0;
  virtual void foreach_always_required_input_index(const NodeID &node,
                                                   FunctionRef<void(int)> fn) const = 0;
  virtual void execute_node(const NodeID &node, ExecuteNodeParams &params) const = 0;
};

enum class ValueUsage : uint8_t {
  Required,
  Maybe,
  Unused,
};

enum class NodeScheduleState {
  NotScheduled,
  Scheduled,
  Running,
  RunningAndRescheduled,
};

struct SingleInputValue {
  void *value = nullptr;
};

template<typename SGraphAdapter> struct MultiInputValueT {
  Vector<LinkT<SGraphAdapter>> links;
  Array<void *> values;
  int provided_value_count = 0;

  int missing_values() const
  {
    return this->values.size() - this->provided_value_count;
  }

  bool all_values_available() const
  {
    return this->values.size() == this->provided_value_count;
  }
};

struct IOIndices {
  int input_index = -1;
  int output_index = -1;
};

template<typename SGraphAdapter> struct InputStateT {
  const CPPType *type = nullptr;

  union {
    SingleInputValue *single;
    MultiInputValueT<SGraphAdapter> *multi;
  } value;

  ValueUsage usage = ValueUsage::Maybe;
  bool was_ready_for_execution = false;
  bool is_destructed = false;
  IOIndices io;
};

struct OutputState {
  const CPPType *type = nullptr;
  ValueUsage usage = ValueUsage::Maybe;
  ValueUsage usage_for_execution = ValueUsage::Maybe;
  int potential_users = 0;
  bool has_been_computed = false;
  IOIndices io;
};

template<typename SGraphAdapter> struct NodeStateT {
  mutable std::mutex mutex;
  MutableSpan<InputStateT<SGraphAdapter>> inputs;
  MutableSpan<OutputState> outputs;

  int missing_required_values = 0;
  bool node_has_finished = false;
  bool always_required_inputs_handled = false;
  NodeScheduleState schedule_state = NodeScheduleState::NotScheduled;
};

template<typename SGraphAdapter> class ExecuteNodeParamsT;

template<typename SGraphAdapter> class SGraphEvaluator {
 private:
  using NodeID = typename SGraphAdapter::NodeID;
  using SGraph = SGraphT<SGraphAdapter>;
  using Node = NodeT<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;
  using Socket = SocketT<SGraphAdapter>;
  using Link = LinkT<SGraphAdapter>;
  using Executor = SGraphExecuteSemantics<SGraphAdapter>;
  using NodeState = NodeStateT<SGraphAdapter>;
  using InputState = InputStateT<SGraphAdapter>;
  using MultiInputValue = MultiInputValueT<SGraphAdapter>;

  friend ExecuteNodeParamsT<SGraphAdapter>;

  LinearAllocator<> allocator_;
  const SGraph graph_;
  const Executor &executor_;
  ExecuteGraphIO &graph_io_;
  const VectorSet<Socket> input_sockets_;
  const VectorSet<Socket> output_sockets_;
  Map<Node, destruct_ptr<NodeState>> node_states_;
  TaskPool *task_pool_ = nullptr;

  threading::EnumerableThreadSpecific<LinearAllocator<>> local_allocators_;

  /** Debug utils. */
  threading::EnumerableThreadSpecific<bool> node_is_locked_by_thread;

  struct LockedNode : NonCopyable, NonMovable {
    Node node;
    NodeState &node_state;

    Vector<OutSocket> delayed_required_outputs;
    Vector<OutSocket> delayed_unused_outputs;
    Vector<Node> delayed_scheduled_nodes;

    LockedNode(Node node, NodeState &node_state) : node(std::move(node)), node_state(node_state)
    {
    }
  };

 public:
  SGraphEvaluator(SGraph graph,
                  const Executor &executor,
                  ExecuteGraphIO &graph_io,
                  const Span<Socket> input_sockets,
                  const Span<Socket> output_sockets)
      : graph_(std::move(graph)),
        executor_(executor),
        graph_io_(graph_io),
        input_sockets_(input_sockets),
        output_sockets_(output_sockets)
  {
    this->initialize_reachable_node_states();
    task_pool_ = BLI_task_pool_create(this, TASK_PRIORITY_HIGH);

#ifdef DEBUG
    for (const Socket &socket : input_sockets_) {
      if (socket.is_input) {
        BLI_assert(!this->is_multi_input(InSocket(socket)));
      }
    }
    for (const Socket &socket : output_sockets_) {
      if (socket.is_input) {
        BLI_assert(!this->is_multi_input(InSocket(socket)));
      }
    }
#endif
  }

  ~SGraphEvaluator()
  {
    BLI_task_pool_free(task_pool_);
  }

  void execute()
  {
    this->schedule_newly_requested_outputs();
    this->forward_newly_provided_inputs();
    BLI_task_pool_work_and_wait(task_pool_);
  }

 private:
  void initialize_reachable_node_states()
  {
    Stack<Node> nodes_to_check;
    for (const Socket &socket : output_sockets_) {
      nodes_to_check.push(socket.node);
    }
    while (!nodes_to_check.is_empty()) {
      const Node node = nodes_to_check.pop();
      if (node_states_.contains(node)) {
        continue;
      }

      destruct_ptr<NodeState> node_state = allocator_.construct<NodeState>();
      node_states_.add_new(node, std::move(node_state));

      for (const int input_index : IndexRange(node.inputs_size(graph_))) {
        const InSocket in_socket = node.input(graph_, input_index);
        in_socket.foreach_linked(graph_, [&](const OutSocket &origin_socket) {
          nodes_to_check.push(origin_socket.node);
        });
      }
    }

    for (auto item : node_states_.items()) {
      const Node node = item.key;
      NodeState &node_state = *item.value;
      node_state.inputs = allocator_.construct_array<InputState>(node.inputs_size(graph_));
      node_state.outputs = allocator_.construct_array<OutputState>(node.outputs_size(graph_));

      for (const int input_index : node_state.inputs.index_range()) {
        InSocket in_socket = node.input(graph_, input_index);
        InputState &input_state = node_state.inputs[input_index];
        input_state.type = executor_.input_socket_type(node.id, input_index);
        if (input_state.type == nullptr) {
          input_state.usage = ValueUsage::Unused;
        }
        else {
          if (this->is_multi_input(node, input_index)) {
            /* TODO: destruct */
            MultiInputValue &multi_value = *allocator_.construct<MultiInputValue>().release();
            input_state.value.multi = &multi_value;
            in_socket.foreach_linked(graph_, [&](const OutSocket &origin_socket) {
              multi_value.links.append(Link{origin_socket, in_socket});
            });
            multi_value.values.reinitialize(multi_value.links.size());
          }
          else {
            input_state.value.single = allocator_.construct<SingleInputValue>().release();
          }
        }
      }
      for (const int output_index : node_state.outputs.index_range()) {
        OutSocket out_socket = node.output(graph_, output_index);
        OutputState &output_state = node_state.outputs[output_index];
        output_state.type = executor_.output_socket_type(node.id, output_index);
        if (output_state.type == nullptr) {
          output_state.usage = ValueUsage::Unused;
        }

        output_state.potential_users = 0;
        out_socket.foreach_linked(graph_, [&](const InSocket &target_socket) {
          if (!node_states_.contains(target_socket.node)) {
            return;
          }
          output_state.potential_users += 1;
        });
        if (output_state.potential_users == 0) {
          output_state.usage = ValueUsage::Unused;
        }
      }
    }

    for (const int io_input_index : input_sockets_.index_range()) {
      const Socket socket = input_sockets_[io_input_index];
      NodeState &node_state = *node_states_.lookup(socket.node);
      IOIndices &io_indices = (socket.is_input) ? node_state.inputs[socket.index].io :
                                                  node_state.outputs[socket.index].io;
      io_indices.input_index = io_input_index;
    }
    for (const int io_output_index : output_sockets_.index_range()) {
      const Socket socket = output_sockets_[io_output_index];
      NodeState &node_state = *node_states_.lookup(socket.node);
      IOIndices &io_indices = (socket.is_input) ? node_state.inputs[socket.index].io :
                                                  node_state.outputs[socket.index].io;
      io_indices.output_index = io_output_index;
    }
  }

  void schedule_newly_requested_outputs()
  {
    for (const int i : output_sockets_.index_range()) {
      if (!graph_io_.output_is_required(i)) {
        continue;
      }
      const Socket socket = output_sockets_[i];
      NodeState &node_state = *node_states_.lookup(socket.node);
      OutputState &output_state = node_state.outputs[socket.index];
      if (output_state.has_been_computed) {
        continue;
      }

      if (socket.is_input) {
        this->with_locked_node(socket.node, node_state, [&](LockedNode &locked_node) {
          this->set_input_required(locked_node, InSocket(socket));
        });
      }
      else {
        this->notify_output_required(OutSocket(socket));
      }
    }
  }

  void forward_newly_provided_inputs()
  {
    LinearAllocator<> &allocator = local_allocators_.local();
    for (const int io_input_index : input_sockets_.index_range()) {
      if (!graph_io_.can_load_input(io_input_index)) {
        continue;
      }
      const Socket socket = input_sockets_[io_input_index];
      destruct_ptr<NodeState> *node_state_ptr = node_states_.lookup_ptr(socket.node);
      if (node_state_ptr == nullptr) {
        /* The value is never used. */
        continue;
      }
      const CPPType &type = *this->get_cpp_type(socket);
      void *buffer = allocator.allocate(type.size(), type.alignment());
      graph_io_.load_input_to_uninitialized(io_input_index, {type, buffer});
      if (socket.is_input) {
        this->forward_value_to_input(InSocket(socket), std::nullopt, {type, buffer});
      }
      else {
        this->forward_output_provided_by_outside(OutSocket(socket), {type, buffer});
      }
    }
  }

  const CPPType *get_cpp_type(const Socket &socket) const
  {
    if (socket.is_input) {
      return executor_.input_socket_type(socket.node.id, socket.index);
    }
    return executor_.output_socket_type(socket.node.id, socket.index);
  }

  void notify_output_required(const OutSocket socket)
  {
    const Node node = socket.node;
    NodeState &node_state = *node_states_.lookup(node);
    OutputState &output_state = node_state.outputs[socket.index];

    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      if (output_state.usage == ValueUsage::Required) {
        return;
      }
      output_state.usage = ValueUsage::Required;
      this->schedule_node(locked_node);
    });
  }

  void notify_output_unused(const OutSocket socket)
  {
    const Node node = socket.node;
    NodeState &node_state = *node_states_.lookup(node);
    OutputState &output_state = node_state.outputs[socket.index];

    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      output_state.potential_users -= 1;
      if (output_state.potential_users == 0) {
        if (output_state.usage != ValueUsage::Required && output_state.io.output_index != -1) {
          output_state.usage = ValueUsage::Unused;
          this->schedule_node(locked_node);
        }
      }
    });
  }

  void schedule_node(LockedNode &locked_node)
  {
    switch (locked_node.node_state.schedule_state) {
      case NodeScheduleState::NotScheduled: {
        locked_node.node_state.schedule_state = NodeScheduleState::Scheduled;
        locked_node.delayed_scheduled_nodes.append(locked_node.node);
        break;
      }
      case NodeScheduleState::Scheduled: {
        break;
      }
      case NodeScheduleState::Running: {
        locked_node.node_state.schedule_state = NodeScheduleState::RunningAndRescheduled;
        break;
      }
      case NodeScheduleState::RunningAndRescheduled: {
        break;
      }
    }
  }

  template<typename F> void with_locked_node(const Node node, NodeState &node_state, const F &f)
  {
    bool &any_node_is_locked_on_current_thread = node_is_locked_by_thread.local();
    if (any_node_is_locked_on_current_thread) {
      BLI_assert_unreachable();
    }

    LockedNode locked_node{node, node_state};
    {
      std::lock_guard lock{node_state.mutex};
      any_node_is_locked_on_current_thread = true;
      threading::isolate_task([&]() { f(locked_node); });
      any_node_is_locked_on_current_thread = false;
    }

    for (const OutSocket &socket : locked_node.delayed_required_outputs) {
      this->notify_output_required(socket);
    }
    for (const OutSocket &socket : locked_node.delayed_unused_outputs) {
      this->notify_output_unused(socket);
    }
    for (const Node &node : locked_node.delayed_scheduled_nodes) {
      this->add_node_to_task_pool(node);
    }
  }

  void add_node_to_task_pool(const Node &node)
  {
    const Node *node_ptr = node_states_.lookup_key_ptr(node);
    BLI_task_pool_push(task_pool_, run_node_from_task_pool, (void *)node_ptr, false, nullptr);
  }

  static void run_node_from_task_pool(TaskPool *task_pool, void *task_data)
  {
    void *user_data = BLI_task_pool_user_data(task_pool);
    SGraphEvaluator &evaluator = *static_cast<SGraphEvaluator *>(user_data);
    const Node &node = *static_cast<const Node *>(task_data);
    evaluator.run_node_task(node);
  }

  void run_node_task(const Node &node)
  {
    NodeState &node_state = *node_states_.lookup(node);
    std::cout << "Run Node Task: " << node.id << "\n";

    bool node_needs_execution = false;
    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      BLI_assert(node_state.schedule_state == NodeScheduleState::Scheduled);
      node_state.schedule_state = NodeScheduleState::Running;

      if (node_state.node_has_finished) {
        return;
      }

      bool required_uncomputed_exists = false;
      for (OutputState &output_state : node_state.outputs) {
        output_state.usage_for_execution = output_state.usage;
        if (output_state.usage == ValueUsage::Required && !output_state.has_been_computed) {
          required_uncomputed_exists = true;
        }
      }
      if (!required_uncomputed_exists) {
        return;
      }

      if (!node_state.always_required_inputs_handled) {
        executor_.foreach_always_required_input_index(node.id, [&](const int input_index) {
          this->set_input_required(locked_node, node.input(graph_, input_index));
        });
        node_state.always_required_inputs_handled = true;
      }

      for (const int input_index : node_state.inputs.index_range()) {
        InputState &input_state = node_state.inputs[input_index];
        if (input_state.type == nullptr) {
          continue;
        }
        if (input_state.was_ready_for_execution) {
          continue;
        }

        if (this->is_multi_input(node, input_index)) {
          MultiInputValue &multi_value = *input_state.value.multi;
          if (multi_value.all_values_available()) {
            input_state.was_ready_for_execution = true;
          }
        }
        else {
          SingleInputValue &single_value = *input_state.value.single;
          if (single_value.value != nullptr) {
            input_state.was_ready_for_execution = true;
          }
        }
        if (!input_state.was_ready_for_execution && input_state.usage == ValueUsage::Required) {
          return;
        }
      }

      node_needs_execution = true;
    });

    if (node_needs_execution) {
      this->execute_node(node, node_state);
    }

    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      this->finish_node_if_possible(locked_node);
      const bool reschedule_requested = node_state.schedule_state ==
                                        NodeScheduleState::RunningAndRescheduled;
      node_state.schedule_state = NodeScheduleState::NotScheduled;
      if (reschedule_requested && !node_state.node_has_finished) {
        this->schedule_node(locked_node);
      }

      if (node_needs_execution) {
        this->assert_expected_outputs_has_been_computed(locked_node);
      }
    });
  }

  void assert_expected_outputs_has_been_computed(LockedNode &locked_node)
  {
    const NodeState &node_state = locked_node.node_state;
    if (node_state.missing_required_values > 0) {
      /* If the node still requires some inputs, it is ok if not all outputs have been computed
       * yet. */
      return;
    }
    if (node_state.schedule_state == NodeScheduleState::Scheduled) {
      /* The node is scheduled again already, so it still has a chance to compute the remaining
       * outputs. */
      return;
    }
    for (const OutputState &output_state : node_state.outputs) {
      if (output_state.usage_for_execution == ValueUsage::Required) {
        BLI_assert(output_state.has_been_computed);
      }
    }
  }

  void finish_node_if_possible(LockedNode &locked_node)
  {
    const Node node = locked_node.node;
    NodeState &node_state = locked_node.node_state;

    if (node_state.node_has_finished) {
      return;
    }

    for (OutputState &output_state : node_state.outputs) {
      if (output_state.usage != ValueUsage::Unused && !output_state.has_been_computed) {
        return;
      }
    }

    for (InputState &input_state : node_state.inputs) {
      if (input_state.usage == ValueUsage::Required && !input_state.was_ready_for_execution) {
        return;
      }
    }

    node_state.node_has_finished = true;

    for (const int input_index : node_state.inputs.index_range()) {
      const InSocket socket = node.input(graph_, input_index);
      InputState &input_state = node_state.inputs[input_index];
      if (input_state.usage == ValueUsage::Maybe) {
        this->set_input_unused(locked_node, socket);
      }
      else if (input_state.usage == ValueUsage::Required) {
        this->destruct_input_value_if_exists(locked_node, socket);
      }
    }
  }

  void destruct_input_value_if_exists(LockedNode &locked_node, const InSocket in_socket)
  {
    NodeState &node_state = locked_node.node_state;
    InputState &input_state = node_state.inputs[in_socket.index];
    if (input_state.type == nullptr) {
      return;
    }
    const CPPType &type = *input_state.type;
    if (this->is_multi_input(in_socket)) {
      MultiInputValue &multi_value = *input_state.value.multi;
      for (void *&buffer : multi_value.values) {
        if (buffer != nullptr) {
          type.destruct(buffer);
          buffer = nullptr;
        }
      }
    }
    else {
      SingleInputValue &single_value = *input_state.value.single;
      if (single_value.value != nullptr) {
        type.destruct(single_value.value);
        single_value.value = nullptr;
      }
    }
  }

  void execute_node(const Node node, NodeState &node_state);

  void set_input_unused_during_execution(const Node node,
                                         NodeState &node_state,
                                         const int input_index)
  {
    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      this->set_input_unused(locked_node, node.input(graph_, input_index));
    });
  }

  void set_input_unused(LockedNode &locked_node, const InSocket in_socket)
  {
    NodeState &node_state = locked_node.node_state;
    InputState &input_state = node_state.inputs[in_socket.index];

    /* A required socket cannot become unused. */
    BLI_assert(input_state.usage != ValueUsage::Required);

    if (input_state.usage == ValueUsage::Unused) {
      /* Nothing to do. */
      return;
    }
    input_state.usage = ValueUsage::Unused;

    /* The value of an unused input is never used again, so it can be destructed now. */
    this->destruct_input_value_if_exists(locked_node, in_socket);

    if (input_state.was_ready_for_execution) {
      /* If the value was already computed, the origin nodes don't need to be notified. */
      return;
    }

    /* Let the origin sockets know that they may become unused as well. */
    in_socket.foreach_linked(graph_, [&](const OutSocket &origin) {
      /* Delay notification of the other nodes until this node is not locked anymore. */
      locked_node.delayed_unused_outputs.append(origin);
    });
  }

  LazyRequireInputResult set_input_required_during_execution(const Node node,
                                                             NodeState &node_state,
                                                             const int input_index)
  {
    LazyRequireInputResult result;
    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      result = this->set_input_required(locked_node, node.input(graph_, input_index));
      if (result == LazyRequireInputResult::Ready) {
        this->schedule_node(locked_node);
      }
    });
    return result;
  }

  LazyRequireInputResult set_input_required(LockedNode &locked_node, const InSocket in_socket)
  {
    BLI_assert(locked_node.node == in_socket.node);
    Node &node = locked_node.node;
    NodeState &node_state = locked_node.node_state;
    InputState &input_state = node_state.inputs[in_socket.index];

    /* A socket that is marked unused cannot become required again. */
    BLI_assert(input_state.usage != ValueUsage::Unused);

    if (input_state.was_ready_for_execution) {
      /* The value was ready before. Either it is still available or it has been consumed already.
       * In the latter case it can not be computed a second time. */
      return LazyRequireInputResult::Ready;
    }

    if (input_state.usage == ValueUsage::Required) {
      /* The socket was required and not ready before. Just stay in that state and wait until the
       * node is notified when the value becomes available. */
      return LazyRequireInputResult::NotYetAvailable;
    }
    input_state.usage = ValueUsage::Required;

    /* A new input has become required, so increase the number of missing required values. */
    if (this->is_multi_input(locked_node.node, in_socket.index)) {
      MultiInputValue &multi_value = *input_state.value.multi;
      node_state.missing_required_values += multi_value.missing_values();
    }
    else {
      SingleInputValue &single_value = *input_state.value.single;
      BLI_assert(single_value.value == nullptr);
      UNUSED_VARS_NDEBUG(single_value);
      node_state.missing_required_values += 1;
    }

    if (input_state.io.input_index != -1) {
      /* The input socket is overridden from the outside, so request the value from there. */
      graph_io_.require_input(input_state.io.input_index);
      return LazyRequireInputResult::NotYetAvailable;
    }

    Vector<OutSocket> origin_sockets;
    in_socket.foreach_linked(
        graph_, [&](const OutSocket &origin_socket) { origin_sockets.append(origin_socket); });

    if (origin_sockets.is_empty()) {
      if (this->is_multi_input(in_socket)) {
        /* Must be empty, otherwise there would be origin sockets. */
        BLI_assert(input_state.value.multi->values.is_empty());
      }
      else {
        /* Load the value. */
        const CPPType &type = *input_state.type;
        void *buffer = allocator_.allocate(type.size(), type.alignment());
        executor_.load_unlinked_single_input(node.id, in_socket.index, {type, buffer});
        input_state.value.single->value = buffer;

        if (input_state.io.output_index != -1) {
          /* The input socket is also an output of the entire graph. */
          graph_io_.set_output_by_copy(input_state.io.output_index, {type, buffer});
        }
      }
      node_state.missing_required_values -= 1;
      input_state.was_ready_for_execution = true;
      return LazyRequireInputResult::Ready;
    }

    /* The origin values still have to be computed. */
    for (const OutSocket &origin_socket : origin_sockets) {
      locked_node.delayed_required_outputs.append(origin_socket);
    }

    return LazyRequireInputResult::NotYetAvailable;
  }

  bool is_multi_input(const InSocket socket) const
  {
    return this->is_multi_input(socket.node, socket.index);
  }

  bool is_multi_input(const Node node, const int input_index) const
  {
    return executor_.is_multi_input(node.id, input_index);
  }

  void forward_output_provided_by_outside(const OutSocket from_socket,
                                          GMutablePointer value_to_forward)
  {
    const int io_output_index = output_sockets_.index_of_try(from_socket);
    if (io_output_index != -1) {
      /* Same socket is used as input and output. */
      graph_io_.set_output_by_copy(io_output_index, value_to_forward);
    }
    this->forward_value_to_linked_inputs(from_socket, value_to_forward);
  }

  void forward_computed_node_output(const OutSocket from_socket, GMutablePointer value_to_forward)
  {
    BLI_assert(value_to_forward.get() != nullptr);

    const int io_input_index = input_sockets_.index_of_try(from_socket);
    const int io_output_index = output_sockets_.index_of_try(from_socket);

    if (io_input_index != -1) {
      /* The computed value is ignored, because it is overridden from the outside. */
      value_to_forward.destruct();
      return;
    }
    if (io_output_index != -1) {
      /* Report computed value to the outside. */
      graph_io_.set_output_by_copy(io_output_index, value_to_forward);
    }

    this->forward_value_to_linked_inputs(from_socket, value_to_forward);
  }

  void forward_value_to_linked_inputs(const OutSocket from_socket,
                                      GMutablePointer value_to_forward)
  {
    LinearAllocator<> &allocator = local_allocators_.local();
    Vector<InSocket> sockets_to_forward_to;
    Vector<GMutablePointer> forwarded_values;
    from_socket.foreach_linked(graph_, [&](const InSocket &to_socket) {
      const destruct_ptr<NodeState> *node_state_ptr = node_states_.lookup_ptr(to_socket.node);
      if (node_state_ptr == nullptr) {
        return;
      }
      const NodeState &node_state = **node_state_ptr;
      const InputState &input_state = node_state.inputs[to_socket.index];
      if (input_state.type == nullptr) {
        return;
      }
      if (input_state.io.input_index != -1) {
        return;
      }
      {
        std::lock_guard lock{node_state.mutex};
        if (input_state.usage == ValueUsage::Unused) {
          return;
        }
      }
      const CPPType &type = *input_state.type;
      void *forwarded_buffer = allocator.allocate(type.size(), type.alignment());
      forwarded_values.append({type, forwarded_buffer});
      sockets_to_forward_to.append(to_socket);
    });

    for (const int i : forwarded_values.index_range()) {
      /* TODO */
      *forwarded_values[i].get<int>() = *value_to_forward.get<int>();
    }

    for (const int i : forwarded_values.index_range()) {
      this->forward_value_to_input(sockets_to_forward_to[i], from_socket, forwarded_values[i]);
    }
  }

  void forward_value_to_input(const InSocket socket,
                              const std::optional<OutSocket> origin,
                              GMutablePointer value)
  {
    NodeState &node_state = *node_states_.lookup(socket.node);
    InputState &input_state = node_state.inputs[socket.index];
    BLI_assert(*value.type() == *input_state.type);

    if (input_state.usage == ValueUsage::Unused) {
      value.destruct();
      return;
    }

    this->with_locked_node(socket.node, node_state, [&](LockedNode &locked_node) {
      if (this->is_multi_input(socket)) {
        MultiInputValue &multi_value = *input_state.value.multi;
        BLI_assert(origin.has_value());
        int origin_index = -1;
        for (const int i : multi_value.links.index_range()) {
          const Link &link = multi_value.links[i];
          if (link.from == *origin) {
            origin_index = i;
            break;
          }
        }
        BLI_assert(multi_value.values[origin_index] == nullptr);
        multi_value.values[origin_index] = value.get();
      }
      else {
        SingleInputValue &single_value = *input_state.value.single;
        BLI_assert(single_value.value == nullptr);
        BLI_assert(!input_state.was_ready_for_execution);
        single_value.value = value.get();

        if (input_state.io.output_index != -1) {
          graph_io_.set_output_by_copy(input_state.io.output_index, value);
        }
      }
      if (input_state.usage == ValueUsage::Required) {
        node_state.missing_required_values--;
        if (node_state.missing_required_values == 0) {
          this->schedule_node(locked_node);
        }
      }
    });
  }
};

template<typename SGraphAdapter> class ExecuteNodeParamsT final : public ExecuteNodeParams {
 private:
  using Evaluator = SGraphEvaluator<SGraphAdapter>;
  using Node = NodeT<SGraphAdapter>;
  using NodeState = NodeStateT<SGraphAdapter>;
  using InputState = InputStateT<SGraphAdapter>;
  friend Evaluator;

  Evaluator &evaluator_;
  Node node_;
  NodeState &node_state_;

  ExecuteNodeParamsT(Evaluator &evaluator, Node node, NodeState &node_state)
      : evaluator_(evaluator), node_(std::move(node)), node_state_(node_state)
  {
  }

 public:
  bool is_input_available(const int index) const override
  {
    const InputState &input_state = node_state_.inputs[index];
    if (!input_state.was_ready_for_execution) {
      return false;
    }
    if (input_state.is_destructed) {
      return false;
    }
    return true;
  }

  bool output_was_set(int index) const override
  {
    const OutputState &output_state = node_state_.outputs[index];
    return output_state.has_been_computed;
  }

  GMutablePointer extract_single_input(int index) override
  {
    BLI_assert(!evaluator_.is_multi_input(node_, index));
    BLI_assert(this->is_input_available(index));

    InputState &input_state = node_state_.inputs[index];
    SingleInputValue &single_value = *input_state.value.single;
    void *value = single_value.value;
    single_value.value = nullptr;
    return {*input_state.type, value};
  }

  GPointer get_input(int index) const override
  {
    BLI_assert(!evaluator_.is_multi_input(node_, index));
    BLI_assert(this->is_input_available(index));

    const InputState &input_state = node_state_.inputs[index];
    const SingleInputValue &single_value = *input_state.value.single;
    return {*input_state.type, single_value.value};
  }

  void set_output_by_copy(int index, GPointer value) override
  {
    OutputState &output_state = node_state_.outputs[index];
    const CPPType &type = *output_state.type;

    BLI_assert(!this->output_was_set(index));
    BLI_assert(*value.type() == type);

    output_state.has_been_computed = true;

    LinearAllocator<> &allocator = evaluator_.local_allocators_.local();
    void *buffer = allocator.allocate(type.size(), type.alignment());
    type.copy_construct(value.get(), buffer);
    evaluator_.forward_computed_node_output(node_.output(evaluator_.graph_, index),
                                            {type, buffer});
  }

  void set_output_by_move(int index, GMutablePointer value) override
  {
    OutputState &output_state = node_state_.outputs[index];
    const CPPType &type = *output_state.type;

    BLI_assert(!this->output_was_set(index));
    BLI_assert(*value.type() == type);

    output_state.has_been_computed = true;

    LinearAllocator<> &allocator = evaluator_.local_allocators_.local();
    void *buffer = allocator.allocate(type.size(), type.alignment());
    type.move_construct(value.get(), buffer);
    evaluator_.forward_computed_node_output(node_.output(evaluator_.graph_, index),
                                            {type, buffer});
  }

  bool output_maybe_required(int index) const override
  {
    const OutputState &output_state = node_state_.outputs[index];
    return output_state.usage_for_execution != ValueUsage::Unused;
  }

  LazyRequireInputResult set_input_required(int index) override
  {
    return evaluator_.set_input_required_during_execution(node_, node_state_, index);
  }

  virtual void set_input_unused(int index) override
  {
    evaluator_.set_input_unused_during_execution(node_, node_state_, index);
  }

  bool output_is_required(int index) override
  {
    const OutputState &output_state = node_state_.outputs[index];
    return output_state.usage_for_execution == ValueUsage::Required;
  }
};

template<typename SGraphAdapter>
void SGraphEvaluator<SGraphAdapter>::execute_node(const NodeT<SGraphAdapter> node,
                                                  NodeState &node_state)
{
  ExecuteNodeParamsT<SGraphAdapter> execute_params{*this, node, node_state};
  executor_.execute_node(node.id, execute_params);
}

}  // namespace blender::fn::sgraph
