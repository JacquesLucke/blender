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

/**
 * This is passed to the node execution function. It has a concrete implementation below. An
 * abstract base type is used for type erasure.
 */
class ExecuteNodeParams {
 public:
  virtual bool is_input_available(int index) const = 0;
  virtual bool output_was_set(int index) const = 0;

  virtual GMutablePointer extract_single_input(int index) = 0;
  virtual GPointer get_input(int index) const = 0;

  virtual void set_output_by_move(int index, GMutablePointer value) = 0;

  virtual bool output_maybe_required(int index) const = 0;

  virtual LazyRequireInputResult set_input_required(int index) = 0;
  virtual void set_input_unused(int index) = 0;
  virtual bool output_is_required(int index) = 0;
};

/**
 * Determines how data enters and exists the graph.
 * This has to be implemented by the code that wants to evaluate a graph.
 */
class ExecuteGraphIO {
 public:
  virtual LazyRequireInputResult require_input(int index) = 0;
  virtual void set_input_unused(int index) = 0;
  virtual void load_input_to_uninitialized(int index, GMutablePointer r_value) = 0;
  virtual bool can_load_input(int index) const = 0;
  virtual bool output_is_required(int index) const = 0;
  virtual void set_output_by_copy(int index, GPointer value) = 0;
};

/**
 * Determines how a graph is evaluated.
 * This has to be implemented by the code that wants to evaluate a graph.
 */
template<typename SGraphAccessor> class SGraphExecuteSemantics {
 public:
  using Node = typename SGraphAccessor::Types::Node;
  using Socket = typename SGraphAccessor::Types::Socket;
  using InSocket = typename SGraphAccessor::Types::InSocket;

  virtual const CPPType *socket_type(const Socket &socket) const = 0;
  virtual void load_unlinked_single_input(const InSocket &socket,
                                          GMutablePointer r_value) const = 0;
  virtual bool is_multi_input(const InSocket &socket) const = 0;
  virtual void foreach_always_required_input_index(const Node &node,
                                                   FunctionRef<void(int)> fn) const = 0;
  virtual void execute_node(const Node &node, ExecuteNodeParams &params) const = 0;
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

template<typename SGraphAccessor> struct MultiInputValueT {
  using Link = typename SGraphAccessor::Types::Link;
  Vector<Link> links;
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

template<typename SGraphAccessor> struct InputStateT {
  const CPPType *type = nullptr;

  union {
    SingleInputValue *single;
    MultiInputValueT<SGraphAccessor> *multi;
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

template<typename SGraphAccessor> struct NodeStateT {
  mutable std::mutex mutex;
  MutableSpan<InputStateT<SGraphAccessor>> inputs;
  MutableSpan<OutputState> outputs;

  int missing_required_values = 0;
  bool node_has_finished = false;
  bool always_required_inputs_handled = false;
  NodeScheduleState schedule_state = NodeScheduleState::NotScheduled;
};

template<typename SGraphAccessor> class ExecuteNodeParamsT;

template<typename SGraphAccessor> class SGraphEvaluator {
 private:
  using SGraph = SGraphT<SGraphAccessor>;
  using Node = typename SGraphAccessor::Node;
  using Link = typename SGraphAccessor::Types::Link;
  using Socket = typename SGraphAccessor::Types::Socket;
  using InSocket = typename SGraphAccessor::Types::InSocket;
  using OutSocket = typename SGraphAccessor::Types::OutSocket;

  using Executor = SGraphExecuteSemantics<SGraphAccessor>;
  using NodeState = NodeStateT<SGraphAccessor>;
  using InputState = InputStateT<SGraphAccessor>;
  using MultiInputValue = MultiInputValueT<SGraphAccessor>;

  friend ExecuteNodeParamsT<SGraphAccessor>;

  LinearAllocator<> allocator_;
  const SGraph &graph_;
  const Executor &executor_;
  ExecuteGraphIO &graph_io_;
  const VectorSet<Socket> input_sockets_;
  const VectorSet<Socket> output_sockets_;
  Map<Node, NodeState *> node_states_;
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
  SGraphEvaluator(const SGraph &graph,
                  const Executor &executor,
                  ExecuteGraphIO &graph_io,
                  const Span<Socket> input_sockets,
                  const Span<Socket> output_sockets)
      : graph_(graph),
        executor_(executor),
        graph_io_(graph_io),
        input_sockets_(input_sockets),
        output_sockets_(output_sockets)
  {
    this->initialize_reachable_node_states();
    task_pool_ = BLI_task_pool_create(this, TASK_PRIORITY_HIGH);

#ifdef DEBUG
    for (const Socket &socket : input_sockets_) {
      if (graph.socket_is_input(socket)) {
        BLI_assert(!this->is_multi_input(InSocket(socket)));
      }
    }
    for (const Socket &socket : output_sockets_) {
      if (graph.socket_is_input(socket)) {
        BLI_assert(!this->is_multi_input(InSocket(socket)));
      }
    }
#endif
  }

  ~SGraphEvaluator()
  {
    BLI_task_pool_free(task_pool_);
    for (NodeState *node_state : node_states_.values()) {
      std::destroy_at(node_state);
    }
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
    /* Find reachable nodes and allocate their node state. */
    Stack<Node> nodes_to_check;
    for (const Socket &socket : output_sockets_) {
      nodes_to_check.push(socket.node);
    }
    while (!nodes_to_check.is_empty()) {
      const Node node = nodes_to_check.pop();
      if (node_states_.contains(node)) {
        continue;
      }

      NodeState &node_state = *allocator_.construct<NodeState>().release();
      node_states_.add_new(node, &node_state);

      graph_.foreach_node_input(node, [&](const InSocket &in_socket) {
        graph_.foreach_node_linked_to_input(
            in_socket, [&](const Node &from_node) { nodes_to_check.push(from_node); });
      });
    }

    /* Fill node states with initial data. */
    for (auto item : node_states_.items()) {
      const Node node = item.key;
      NodeState &node_state = *item.value;
      node_state.inputs = allocator_.construct_array<InputState>(node.inputs_size(graph_));
      node_state.outputs = allocator_.construct_array<OutputState>(node.outputs_size(graph_));

      /* Fill input states. */
      int input_index = -1;
      graph_.foreach_node_input(node, [&](const InSocket &in_socket) {
        input_index++;
        InputState &input_state = node_state.inputs[input_index];
        input_state.type = executor_.input_socket_type(in_socket);
        if (input_state.type == nullptr) {
          input_state.usage = ValueUsage::Unused;
        }
        else {
          if (this->is_multi_input(in_socket)) {
            /* TODO: destruct */
            MultiInputValue &multi_value = *allocator_.construct<MultiInputValue>().release();
            input_state.value.multi = &multi_value;
            graph_.foreach_link_to_input(
                in_socket, [&](const Link &link) { multi_value.links.append(link); });
            multi_value.values.reinitialize(multi_value.links.size());
          }
          else {
            input_state.value.single = allocator_.construct<SingleInputValue>().release();
          }
        }
      });

      /* Fill output states. */
      int output_index = -1;
      graph_.foreach_node_output(node, [&](const OutSocket &out_socket) {
        output_index++;
        OutputState &output_state = node_state.outputs[output_index];
        output_state.type = executor_.output_socket_type(out_socket);
        if (output_state.type == nullptr) {
          output_state.usage = ValueUsage::Unused;
        }

        output_state.potential_users = 0;
        graph_.foreach_link_from_output(out_socket, [&](const Link &link) {
          const Node &target_node = graph_.link_to_node(link);
          if (!node_states_.contains(target_node)) {
            return;
          }
          output_state.potential_users += 1;
        });
        if (output_state.potential_users == 0) {
          output_state.usage = ValueUsage::Unused;
        }
      });
    }

    /* Write information about graph inputs to node states. */
    for (const int io_input_index : input_sockets_.index_range()) {
      const Socket &socket = input_sockets_[io_input_index];
      const Node &node = graph_.node_of_socket(socket);
      NodeState *node_state = node_states_.lookup_default(node, nullptr);
      if (node_state == nullptr) {
        graph_io_.set_input_unused(io_input_index);
      }
      else {
        if (graph_.socket_is_input(socket)) {
          const InSocket &in_socket = graph_.socket_to_input(socket);
          const int index = graph_.index_of_input(in_socket);
          node_state->inputs[index].io.input_index = io_input_index;
        }
        else {
          const OutSocket &out_socket = graph_.socket_to_output(socket);
          const int index = graph_.index_of_output(out_socket);
          node_state->outputs[index].io.input_index = io_input_index;
        }
      }
    }
    /* Write information about graph outputs to node states. */
    for (const int io_output_index : output_sockets_.index_range()) {
      const Socket &socket = output_sockets_[io_output_index];
      const Node &node = graph_.node_of_socket(socket);
      NodeState &node_state = *node_states_.lookup(socket.node);
      if (graph_.socket_is_input(socket)) {
        const InSocket &in_socket = graph_.socket_to_input(socket);
        const int index = graph_.index_of_input(in_socket);
        node_state.inputs[index].io.output_index = io_output_index;
      }
      else {
        const OutSocket &out_socket = graph_.socket_to_output(socket);
        const int index = graph_.index_of_output(out_socket);
        OutputState &output_state = node_state.outputs[index];
        output_state.io.output_index = io_output_index;
        /* Set to #Maybe in case it has been set to #Unused before. */
        output_state.usage = ValueUsage::Maybe;
      }
    }
  }

  void schedule_newly_requested_outputs()
  {
    for (const int i : output_sockets_.index_range()) {
      if (!graph_io_.output_is_required(i)) {
        continue;
      }
      const Socket &socket = output_sockets_[i];
      const Node &node = graph_.node_of_socket(socket);
      NodeState &node_state = *node_states_.lookup(node);

      if (graph_.socket_is_input(socket)) {
        const InSocket &in_socket = graph_.socket_to_input(socket);
        this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
          this->set_input_required(locked_node, in_socket);
        });
      }
      else {
        const OutSocket &out_socket = graph_.socket_to_output(socket);
        const int out_socket_index = graph_.index_of_output(out_socket);
        OutputState &output_state = node_state.outputs[out_socket_index];
        if (!output_state.has_been_computed) {
          this->notify_output_required(out_socket);
        }
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
      const Socket &socket = input_sockets_[io_input_index];
      const Node &node = graph_.node_of_socket(socket);
      NodeState *node_state = node_states_.lookup_default(node, nullptr);
      if (node_state == nullptr) {
        /* The value is never used. */
        continue;
      }
      const CPPType &type = *executor_.socket_type(socket);
      void *buffer = allocator.allocate(type.size(), type.alignment());
      graph_io_.load_input_to_uninitialized(io_input_index, {type, buffer});
      if (graph_.socket_is_input(socket)) {
        const InSocket &in_socket = graph_.socket_to_input(socket);
        this->forward_value_to_input(in_socket, std::nullopt, {type, buffer});
      }
      else {
        const OutSocket &out_socket = graph_.socket_to_output(socket);
        this->forward_output_provided_by_outside(out_socket, {type, buffer});
      }
    }
  }

  void notify_output_required(const OutSocket &socket)
  {
    const Node &node = graph_.node_of_output(socket);
    const int index = graph_.index_of_output(socket);
    NodeState &node_state = *node_states_.lookup(node);
    OutputState &output_state = node_state.outputs[index];

    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      if (output_state.usage == ValueUsage::Required) {
        return;
      }
      output_state.usage = ValueUsage::Required;
      this->schedule_node(locked_node);
    });
  }

  void notify_output_unused(const OutSocket &socket)
  {
    const Node &node = graph_.node_of_output(socket);
    const int index = graph_.index_of_output(socket);
    NodeState &node_state = *node_states_.lookup(node);
    OutputState &output_state = node_state.outputs[index];

    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      output_state.potential_users -= 1;
      if (output_state.potential_users == 0) {
        BLI_assert(output_state.usage != ValueUsage::Unused);
        if (output_state.usage != ValueUsage::Required && output_state.io.output_index == -1) {
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
        executor_.foreach_always_required_input_index(node, [&](const int input_index) {
          const InSocket &in_socket = graph_.node_input(node, input_index);
          this->set_input_required(locked_node, in_socket);
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

        const InSocket &in_socket = graph_.node_input(node, input_index);
        if (this->is_multi_input(in_socket)) {
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
    const Node &node = locked_node.node;
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
      const InSocket &in_socket = graph_.node_input(node, input_index);
      InputState &input_state = node_state.inputs[input_index];
      if (input_state.usage == ValueUsage::Maybe) {
        this->set_input_unused(locked_node, in_socket);
      }
      else if (input_state.usage == ValueUsage::Required) {
        this->destruct_input_value_if_exists(locked_node, in_socket);
      }
    }
  }

  void destruct_input_value_if_exists(LockedNode &locked_node, const InSocket &in_socket)
  {
    NodeState &node_state = locked_node.node_state;
    const int in_index = graph_.index_of_input(in_socket);
    InputState &input_state = node_state.inputs[in_index];
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

  void execute_node(const Node &node, NodeState &node_state);

  void set_input_unused_during_execution(const Node &node,
                                         NodeState &node_state,
                                         const int input_index)
  {
    const InSocket &in_socket = graph_.node_input(node, input_index);
    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      this->set_input_unused(locked_node, in_socket);
    });
  }

  void set_input_unused(LockedNode &locked_node, const InSocket &in_socket)
  {
    NodeState &node_state = locked_node.node_state;
    const int in_index = graph_.index_of_input(in_socket);
    InputState &input_state = node_state.inputs[in_index];

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
    graph_.foreach_link_to_input(in_socket, [&](const Link &link) {
      const OutSocket &origin = graph_.link_from_socket(link);
      /* Delay notification of the other nodes until this node is not locked anymore. */
      locked_node.delayed_unused_outputs.append(origin);
    });
  }

  LazyRequireInputResult set_input_required_during_execution(const Node &node,
                                                             NodeState &node_state,
                                                             const int input_index)
  {
    const InSocket &in_socket = graph_.node_input(node, input_index);
    LazyRequireInputResult result;
    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      result = this->set_input_required(locked_node, in_socket);
      if (result == LazyRequireInputResult::Ready) {
        this->schedule_node(locked_node);
      }
    });
    return result;
  }

  LazyRequireInputResult set_input_required(LockedNode &locked_node, const InSocket &in_socket)
  {
    BLI_assert(locked_node.node == graph_.node_of_input(in_socket));
    Node &node = locked_node.node;
    NodeState &node_state = locked_node.node_state;
    const int in_index = graph_.index_of_input(in_socket);
    InputState &input_state = node_state.inputs[in_index];

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
    if (this->is_multi_input(in_socket)) {
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
    graph_.foreach_link_to_input(in_socket, [&](const Link &link) {
      const OutSocket &origin = graph_.link_from_socket(link);
      origin_sockets.append(origin);
    });

    if (origin_sockets.is_empty()) {
      if (this->is_multi_input(in_socket)) {
        /* Must be empty, otherwise there would be origin sockets. */
        BLI_assert(input_state.value.multi->values.is_empty());
      }
      else {
        /* Load the value. */
        const CPPType &type = *input_state.type;
        void *buffer = allocator_.allocate(type.size(), type.alignment());
        executor_.load_unlinked_single_input(in_socket, {type, buffer});
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

  bool is_multi_input(const InSocket &socket) const
  {
    return executor_.is_multi_input(socket);
  }

  bool is_multi_input(const Node &node, const int input_index) const
  {
    const InSocket &socket = graph_.node_input(node, input_index);
    return this->is_multi_input(socket);
  }

  void forward_output_provided_by_outside(const OutSocket &from_socket,
                                          GMutablePointer value_to_forward)
  {
    const int io_output_index = output_sockets_.index_of_try(from_socket);
    if (io_output_index != -1) {
      /* Same socket is used as input and output. */
      graph_io_.set_output_by_copy(io_output_index, value_to_forward);
    }
    this->forward_value_to_linked_inputs(from_socket, value_to_forward);
  }

  void forward_computed_node_output(const OutSocket &from_socket, GMutablePointer value_to_forward)
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

  void forward_value_to_linked_inputs(const OutSocket &from_socket,
                                      GMutablePointer value_to_forward)
  {
    LinearAllocator<> &allocator = local_allocators_.local();
    Vector<Link> links_to_forward_through;
    const CPPType &type = *value_to_forward.type();
    graph_.foreach_link_from_output(from_socket, [&](const Link &link) {
      const InSocket &to_socket = graph_.link_to_socket(link);
      const int to_socket_index = graph_.index_of_input(to_socket);
      const Node &to_node = graph_.node_of_input(to_socket);
      const NodeState *node_state = node_states_.lookup_default(to_node, nullptr);
      if (node_state == nullptr) {
        return;
      }
      const InputState &input_state = node_state->inputs[to_socket_index];
      if (input_state.type == nullptr) {
        return;
      }
      if (input_state.io.input_index != -1) {
        return;
      }

      links_to_forward_through.append(link);
    });

    if (links_to_forward_through.is_empty()) {
      value_to_forward.destruct();
      return;
    }

    for (const int forward_i : links_to_forward_through.index_range()) {
      const Link &link = links_to_forward_through[forward_i];
      const InSocket &to_socket = graph_.link_to_socket(link);
      GMutablePointer value_to_forward_through_link;
      if (forward_i == links_to_forward_through.size() - 1) {
        value_to_forward_through_link = value_to_forward;
      }
      else {
        void *buffer = allocator.allocate(type.size(), type.alignment());
        type.copy_construct(value_to_forward.get(), buffer);
        value_to_forward_through_link = {type, buffer};
      }
      this->forward_value_to_input(to_socket, link, value_to_forward_through_link);
    }
  }

  void forward_value_to_input(const InSocket &to_socket,
                              const std::optional<Link> &from_link,
                              GMutablePointer value)
  {
    NodeState &node_state = *node_states_.lookup(to_socket.node);
    const Node &to_node = graph_.node_of_input(to_socket);
    const int to_socket_index = graph_.index_of_input(to_socket);
    InputState &input_state = node_state.inputs[to_socket_index];
    BLI_assert(*value.type() == *input_state.type);

    this->with_locked_node(to_socket.node, node_state, [&](LockedNode &locked_node) {
      if (input_state.usage == ValueUsage::Unused) {
        value.destruct();
        return;
      }
      if (this->is_multi_input(to_socket)) {
        MultiInputValue &multi_value = *input_state.value.multi;
        int link_index = -1;
        const Link &link = *from_link;
        BLI_assert(link.has_value());
        for (const int i : multi_value.links.index_range()) {
          const Link &link_at_index = multi_value.links[i];
          if (link_at_index == link) {
            link_index = i;
            break;
          }
        }
        BLI_assert(multi_value.values[link_index] == nullptr);
        multi_value.values[link_index] = value.get();
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

template<typename SGraphAccessor> class ExecuteNodeParamsT final : public ExecuteNodeParams {
 private:
  using Evaluator = SGraphEvaluator<SGraphAccessor>;
  using Node = typename SGraphAccessor::Types::Node;
  using OutSocket = typename SGraphAccessor::Types::OutSocket;
  using NodeState = NodeStateT<SGraphAccessor>;
  using InputState = InputStateT<SGraphAccessor>;

  friend Evaluator;

  Evaluator &evaluator_;
  const Node &node_;
  NodeState &node_state_;

  ExecuteNodeParamsT(Evaluator &evaluator, const Node &node, NodeState &node_state)
      : evaluator_(evaluator), node_(node), node_state_(node_state)
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

  bool output_was_set(const int index) const override
  {
    const OutputState &output_state = node_state_.outputs[index];
    return output_state.has_been_computed;
  }

  GMutablePointer extract_single_input(const int index) override
  {
    BLI_assert(!evaluator_.is_multi_input(node_, index));
    BLI_assert(this->is_input_available(index));

    InputState &input_state = node_state_.inputs[index];
    SingleInputValue &single_value = *input_state.value.single;
    void *value = single_value.value;
    single_value.value = nullptr;
    return {*input_state.type, value};
  }

  GPointer get_input(const int index) const override
  {
    BLI_assert(!evaluator_.is_multi_input(node_, index));
    BLI_assert(this->is_input_available(index));

    const InputState &input_state = node_state_.inputs[index];
    const SingleInputValue &single_value = *input_state.value.single;
    return {*input_state.type, single_value.value};
  }

  void set_output_by_move(const int index, GMutablePointer value) override
  {
    OutputState &output_state = node_state_.outputs[index];
    const CPPType &type = *output_state.type;

    BLI_assert(!this->output_was_set(index));
    BLI_assert(*value.type() == type);

    output_state.has_been_computed = true;

    LinearAllocator<> &allocator = evaluator_.local_allocators_.local();
    void *buffer = allocator.allocate(type.size(), type.alignment());
    type.move_construct(value.get(), buffer);
    const OutSocket &out_socket = evaluator_.graph_.node_output(node_, index);
    evaluator_.forward_computed_node_output(out_socket, {type, buffer});
  }

  bool output_maybe_required(const int index) const override
  {
    const OutputState &output_state = node_state_.outputs[index];
    return output_state.usage_for_execution != ValueUsage::Unused;
  }

  LazyRequireInputResult set_input_required(const int index) override
  {
    return evaluator_.set_input_required_during_execution(node_, node_state_, index);
  }

  virtual void set_input_unused(const int index) override
  {
    evaluator_.set_input_unused_during_execution(node_, node_state_, index);
  }

  bool output_is_required(const int index) override
  {
    const OutputState &output_state = node_state_.outputs[index];
    return output_state.usage_for_execution == ValueUsage::Required;
  }
};

template<typename SGraphAccessor>
inline void SGraphEvaluator<SGraphAccessor>::execute_node(const Node &node, NodeState &node_state)
{
  ExecuteNodeParamsT<SGraphAccessor> execute_params{*this, node, node_state};
  executor_.execute_node(node, execute_params);
}

}  // namespace blender::fn::sgraph
