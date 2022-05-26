/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <mutex>

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_function_ref.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "FN_lazy_function_graph_executor.hh"

namespace blender::fn {

enum class NodeScheduleState {
  /**
   * Default state of every node.
   */
  NotScheduled,
  /**
   * The node has been added to the task pool or is otherwise scheduled to be executed in the
   * future.
   */
  Scheduled,
  /**
   * The node is currently running.
   */
  Running,
  /**
   * The node is running and has been rescheduled while running. In this case the node run again.
   * This state exists, because we don't want to add the node to the task pool twice, because then
   * the node might run twice at the same time, which is not allowed. Instead, once the node is
   * done running, it will reschedule itself.
   */
  RunningAndRescheduled,
};

struct InputState {
  /**
   * Value of this input socket. By default, the value is empty. When other nodes are done
   * computing their outputs, the computed values will be forwarded to linked input sockets. The
   * value will thenlive here until it is found that it is not needed anymore.
   *
   * If #was_ready_for_execution is true, access does not require holding the node lock.
   */
  void *value = nullptr;
  /**
   * How the node intends to use this input. By default, all inputs may be used. Based on which
   * outputs are used, a node can decide that an input will definitely be used or is never used.
   * This allows freeing values early and avoids unnecessary computations.
   */
  ValueUsage usage = ValueUsage::Maybe;
  /**
   * Set to true once #value is set and will stay true afterwards. Access during execution of a
   * node, does not require holding the node lock.
   */
  bool was_ready_for_execution = false;
};

struct OutputState {
  /**
   * Keeps track of how the output value is used. If a connected input becomes used, this output
   * has to become used as well. The output becomes unused when it is used by no input socket
   * anymore and it's not an output of the graph.
   */
  ValueUsage usage = ValueUsage::Maybe;
  /**
   * This is a copy of #usage that is done right before node execution starts. This is done so that
   * the node gets a consistent view of what outputs are used, even when this changes while the
   * node is running (the node might be reevaluated in that case). Access during execution of a
   * node, does not require holding the node lock.
   */
  ValueUsage usage_for_execution = ValueUsage::Maybe;
  /**
   * Number of linked sockets that might still use the value of this output.
   */
  int potential_target_sockets = 0;
  /**
   * Is set to true once the output has been computed and then stays true. Access does not require
   * holding the node lock.
   */
  bool has_been_computed = false;
  /**
   * Holds the output value for a short period of time while the node is initializing it and before
   * it's forwarded to input sockets. Access does not require holding the node lock.
   */
  void *value = nullptr;
};

struct NodeState {
  /**
   * Needs to be locked when any data in this state is accessed that is not explicitly marked as
   * not needing the lock.
   */
  mutable std::mutex mutex;
  /**
   * States of the individual input and output sockets. One can index into these arrays without
   * locking. However, to access data inside, a lock is needed unless noted otherwise.
   */
  MutableSpan<InputState> inputs;
  MutableSpan<OutputState> outputs;
  /**
   * Counts the number of inputs that still have to be provided to this node, until it should run
   * again. This is used as an optimization so that nodes are not scheduled unnecessarily in many
   * cases.
   */
  int missing_required_inputs = 0;
  /**
   * Is set to true once the node is done with its work, i.e. when all outputs that may be used
   * have been computed.
   */
  bool node_has_finished = false;
  /**
   * Set to true once the node is done running for the first time.
   */
  bool had_initialization = true;
  /**
   * A node is always in one specific schedule state. This helps to ensure that the same node does
   * not run twice at the same time accidentally.
   */
  NodeScheduleState schedule_state = NodeScheduleState::NotScheduled;
  /**
   * Custom storage of the node.
   */
  void *storage = nullptr;
};

/**
 * Utility class that wraps a node whose state is locked. Having this is a separate class is useful
 * because it allows methods to communicate that they expect the node to be locked.
 */
struct LockedNode {
  /**
   * This is the node that is currently locked.
   */
  const LFNode &node;
  NodeState &node_state;

  /**
   * Used to delay notifying (and therefore locking) other nodes until the current node is not
   * locked anymore. This might not be strictly necessary to avoid deadlocks in the current code,
   * but is a good measure to avoid accidentally adding a deadlock later on. By not locking more
   * than one node per thread at a time, deadlocks are avoided.
   *
   * The notifications will be send right after the node is not locked anymore.
   */
  Vector<const LFOutputSocket *> delayed_required_outputs;
  Vector<const LFOutputSocket *> delayed_unused_outputs;
  Vector<const LFFunctionNode *> delayed_scheduled_nodes;

  LockedNode(const LFNode &node, NodeState &node_state) : node(node), node_state(node_state)
  {
  }
};

struct CurrentTask {
  /**
   * The node that should be run on the same thread after the current node is done. This avoids
   * some overhead by skipping a round trip through the task pool.
   */
  std::atomic<const LFFunctionNode *> next_node = nullptr;
  /**
   * Indicates that some node has been added to the task pool.
   */
  std::atomic<bool> added_node_to_pool = false;
};

class GraphExecutorLazyFunctionParams;

class Executor {
 private:
  /**
   * The graph that is evaluated.
   */
  const LazyFunctionGraph &graph_;
  /**
   * Input and output sockets of the entire graph.
   */
  const VectorSet<const LFOutputSocket *> &graph_inputs_;
  const VectorSet<const LFInputSocket *> &graph_outputs_;
  /**
   * Remembers which inputs have been loaded from the caller already, to void loading them twice.
   */
  Array<bool> loaded_inputs_;
  /**
   * State of every node, indexed by #LFNode::index_in_graph.
   */
  Array<NodeState *> node_states_;
  /**
   * Parameters provided by the caller. This is always non-null, while a node is running.
   */
  LazyFunctionParams *params_ = nullptr;
  /**
   * Used to distribute work on separate nodes to separate threads.
   */
  TaskPool *task_pool_ = nullptr;
  /**
   * A separate linear allocator for every thread. We could potentially reuse some memory, but that
   * doesn't seem worth it yet.
   */
  threading::EnumerableThreadSpecific<LinearAllocator<>> local_allocators_;
  /**
   * Set to false when the first execution ends.
   */
  bool is_first_execution_ = true;

  friend GraphExecutorLazyFunctionParams;

 public:
  Executor(const LazyFunctionGraph &graph,
           const VectorSet<const LFOutputSocket *> &graph_inputs,
           const VectorSet<const LFInputSocket *> &graph_outputs)
      : graph_(graph),
        graph_inputs_(graph_inputs),
        graph_outputs_(graph_outputs),
        loaded_inputs_(graph_inputs.size(), false)
  {
    /* The indices are necessary, because they are used as keys in #node_states_. */
    BLI_assert(graph_.node_indices_are_valid());
    this->initialize_node_states();
    task_pool_ = BLI_task_pool_create(this, TASK_PRIORITY_HIGH);
  }

  ~Executor()
  {
    BLI_task_pool_free(task_pool_);
    threading::parallel_for(node_states_.index_range(), 1024, [&](const IndexRange range) {
      for (const int node_index : range) {
        const LFNode &node = *graph_.nodes()[node_index];
        NodeState &node_state = *node_states_[node_index];
        this->destruct_node_state(node, node_state);
      }
    });
  }

  /**
   * Main entry point to the execution of this graph.
   */
  void execute(LazyFunctionParams &params)
  {
    params_ = &params;
    BLI_SCOPED_DEFER([&]() {
      /* Make sure the #params_ pointer is not dangling, even when it shouldn't be accessed by
       * anyone. */
      params_ = nullptr;
      is_first_execution_ = false;
    });

    if (is_first_execution_) {
      this->set_always_unused_graph_inputs();
      this->set_defaulted_graph_outputs();
    }

    CurrentTask current_task;
    this->schedule_newly_requested_outputs(current_task);
    this->forward_newly_provided_inputs(current_task);

    /* Avoid using task pool when there is no parallel work to do. */
    while (!current_task.added_node_to_pool) {
      if (current_task.next_node == nullptr) {
        /* Nothing to do. */
        return;
      }
      const LFFunctionNode &node = *current_task.next_node;
      current_task.next_node = nullptr;
      this->run_node_task(node, current_task);
    }
    if (current_task.next_node != nullptr) {
      this->add_node_to_task_pool(*current_task.next_node);
    }

    BLI_task_pool_work_and_wait(task_pool_);
  }

 private:
  void initialize_node_states()
  {
    Span<const LFNode *> nodes = graph_.nodes();
    node_states_.reinitialize(nodes.size());

    /* Construct all node states in parallel. */
    threading::parallel_for(nodes.index_range(), 256, [&](const IndexRange range) {
      LinearAllocator<> &allocator = local_allocators_.local();
      for (const int i : range) {
        const LFNode &node = *nodes[i];
        NodeState &node_state = *allocator.construct<NodeState>().release();
        node_states_[i] = &node_state;
        this->construct_initial_node_state(allocator, node, node_state);
      }
    });
  }

  void construct_initial_node_state(LinearAllocator<> &allocator,
                                    const LFNode &node,
                                    NodeState &node_state)
  {
    const Span<const LFInputSocket *> node_inputs = node.inputs();
    const Span<const LFOutputSocket *> node_outputs = node.outputs();

    node_state.inputs = allocator.construct_array<InputState>(node_inputs.size());
    node_state.outputs = allocator.construct_array<OutputState>(node_outputs.size());

    for (const int i : node_outputs.index_range()) {
      OutputState &output_state = node_state.outputs[i];
      const LFOutputSocket &output_socket = *node_outputs[i];
      output_state.potential_target_sockets = output_socket.targets().size();
      if (output_state.potential_target_sockets == 0) {
        output_state.usage = ValueUsage::Unused;
      }
    }
  }

  void destruct_node_state(const LFNode &node, NodeState &node_state)
  {
    if (node.is_function()) {
      const LazyFunction &fn = static_cast<const LFFunctionNode &>(node).function();
      if (node_state.storage != nullptr) {
        fn.destruct_storage(node_state.storage);
      }
    }
    for (const int i : node.inputs().index_range()) {
      InputState &input_state = node_state.inputs[i];
      const LFInputSocket &input_socket = node.input(i);
      this->destruct_input_value_if_exists(input_state, input_socket.type());
    }
    std::destroy_at(&node_state);
  }

  void schedule_newly_requested_outputs(CurrentTask &current_task)
  {
    for (const int graph_output_index : graph_outputs_.index_range()) {
      if (params_->get_output_usage(graph_output_index) != ValueUsage::Used) {
        continue;
      }
      if (params_->output_was_set(graph_output_index)) {
        continue;
      }
      const LFInputSocket &socket = *graph_outputs_[graph_output_index];
      const LFNode &node = socket.node();
      NodeState &node_state = *node_states_[node.index_in_graph()];
      this->with_locked_node(node, node_state, current_task, [&](LockedNode &locked_node) {
        this->set_input_required(locked_node, socket);
      });
    }
  }

  void set_defaulted_graph_outputs()
  {
    for (const int graph_output_index : graph_outputs_.index_range()) {
      const LFSocket &socket = *graph_outputs_[graph_output_index];
      if (socket.is_output()) {
        continue;
      }
      const LFInputSocket &input_socket = socket.as_input();
      if (input_socket.origin() != nullptr) {
        continue;
      }
      const CPPType &type = input_socket.type();
      const void *default_value = input_socket.default_value();
      BLI_assert(default_value != nullptr);
      void *output_ptr = params_->get_output_data_ptr(graph_output_index);
      type.copy_construct(default_value, output_ptr);
      params_->output_set(graph_output_index);
    }
  }

  void set_always_unused_graph_inputs()
  {
    for (const int i : graph_inputs_.index_range()) {
      const LFOutputSocket &socket = *graph_inputs_[i];
      const LFNode &node = socket.node();
      const NodeState &node_state = *node_states_[node.index_in_graph()];
      const OutputState &output_state = node_state.outputs[socket.index_in_node()];
      if (output_state.usage == ValueUsage::Unused) {
        params_->set_input_unused(i);
      }
    }
  }

  void forward_newly_provided_inputs(CurrentTask &current_task)
  {
    LinearAllocator<> &allocator = local_allocators_.local();
    for (const int graph_input_index : graph_inputs_.index_range()) {
      if (loaded_inputs_[graph_input_index]) {
        continue;
      }
      void *input_data = params_->try_get_input_data_ptr(graph_input_index);
      if (input_data == nullptr) {
        continue;
      }
      const LFOutputSocket &socket = *graph_inputs_[graph_input_index];
      const CPPType &type = socket.type();
      void *buffer = allocator.allocate(type.size(), type.alignment());
      type.move_construct(input_data, buffer);
      this->forward_value_to_linked_inputs(socket, {type, buffer}, current_task);
      loaded_inputs_[graph_input_index] = true;
    }
  }

  void notify_output_required(const LFOutputSocket &socket, CurrentTask &current_task)
  {
    const LFNode &node = socket.node();
    const int index_in_node = socket.index_in_node();
    NodeState &node_state = *node_states_[node.index_in_graph()];
    OutputState &output_state = node_state.outputs[index_in_node];

    if (node.is_dummy()) {
      const int graph_input_index = graph_inputs_.index_of(&socket);
      params_->try_get_input_data_ptr_or_request(graph_input_index);
      return;
    }

    BLI_assert(node.is_function());
    this->with_locked_node(node, node_state, current_task, [&](LockedNode &locked_node) {
      if (output_state.usage == ValueUsage::Used) {
        return;
      }
      output_state.usage = ValueUsage::Used;
      this->schedule_node(locked_node);
    });
  }

  void notify_output_unused(const LFOutputSocket &socket, CurrentTask &current_task)
  {
    const LFNode &node = socket.node();
    const int index_in_node = socket.index_in_node();
    NodeState &node_state = *node_states_[node.index_in_graph()];
    OutputState &output_state = node_state.outputs[index_in_node];

    this->with_locked_node(node, node_state, current_task, [&](LockedNode &locked_node) {
      output_state.potential_target_sockets -= 1;
      if (output_state.potential_target_sockets == 0) {
        BLI_assert(output_state.usage != ValueUsage::Unused);
        if (output_state.usage == ValueUsage::Maybe) {
          output_state.usage = ValueUsage::Unused;
          if (node.is_dummy()) {
            const int graph_input_index = graph_inputs_.index_of(&socket);
            params_->set_input_unused(graph_input_index);
          }
          else {
            this->schedule_node(locked_node);
          }
        }
      }
    });
  }

  void schedule_node(LockedNode &locked_node)
  {
    BLI_assert(locked_node.node.is_function());
    switch (locked_node.node_state.schedule_state) {
      case NodeScheduleState::NotScheduled: {
        locked_node.node_state.schedule_state = NodeScheduleState::Scheduled;
        locked_node.delayed_scheduled_nodes.append(
            &static_cast<const LFFunctionNode &>(locked_node.node));
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

  void with_locked_node(const LFNode &node,
                        NodeState &node_state,
                        CurrentTask &current_task,
                        const FunctionRef<void(LockedNode &)> f)
  {
    BLI_assert(&node_state == node_states_[node.index_in_graph()]);

    LockedNode locked_node{node, node_state};
    {
      std::lock_guard lock{node_state.mutex};
      threading::isolate_task([&]() { f(locked_node); });
    }

    this->send_output_required_notifications(locked_node.delayed_required_outputs, current_task);
    this->send_output_unused_notifications(locked_node.delayed_unused_outputs, current_task);
    this->schedule_new_nodes(locked_node.delayed_scheduled_nodes, current_task);
  }

  void send_output_required_notifications(const Span<const LFOutputSocket *> sockets,
                                          CurrentTask &current_task)
  {
    for (const LFOutputSocket *socket : sockets) {
      this->notify_output_required(*socket, current_task);
    }
  }

  void send_output_unused_notifications(const Span<const LFOutputSocket *> sockets,
                                        CurrentTask &current_task)
  {
    for (const LFOutputSocket *socket : sockets) {
      this->notify_output_unused(*socket, current_task);
    }
  }

  void schedule_new_nodes(const Span<const LFFunctionNode *> nodes, CurrentTask &current_task)
  {
    for (const LFFunctionNode *node_to_schedule : nodes) {
      const LFFunctionNode *expected = nullptr;
      if (current_task.next_node.compare_exchange_strong(
              expected, node_to_schedule, std::memory_order_relaxed)) {
        continue;
      }
      this->add_node_to_task_pool(*node_to_schedule);
      current_task.added_node_to_pool.store(true, std::memory_order_relaxed);
    }
  }

  void add_node_to_task_pool(const LFNode &node)
  {
    BLI_task_pool_push(
        task_pool_, Executor::run_node_from_task_pool, (void *)&node, false, nullptr);
  }

  static void run_node_from_task_pool(TaskPool *task_pool, void *task_data)
  {
    void *user_data = BLI_task_pool_user_data(task_pool);
    Executor &executor = *static_cast<Executor *>(user_data);
    const LFFunctionNode &node = *static_cast<const LFFunctionNode *>(task_data);

    CurrentTask current_task;
    current_task.next_node = &node;
    while (current_task.next_node != nullptr) {
      const LFFunctionNode &node_to_run = *current_task.next_node;
      current_task.next_node = nullptr;
      executor.run_node_task(node_to_run, current_task);
    }
  }

  void run_node_task(const LFFunctionNode &node, CurrentTask &current_task)
  {
    NodeState &node_state = *node_states_[node.index_in_graph()];
    LinearAllocator<> &allocator = local_allocators_.local();
    const LazyFunction &fn = node.function();

    bool node_needs_execution = false;
    this->with_locked_node(node, node_state, current_task, [&](LockedNode &locked_node) {
      BLI_assert(node_state.schedule_state == NodeScheduleState::Scheduled);
      node_state.schedule_state = NodeScheduleState::Running;

      if (node_state.node_has_finished) {
        return;
      }

      bool required_uncomputed_output_exists = false;
      for (OutputState &output_state : node_state.outputs) {
        output_state.usage_for_execution = output_state.usage;
        if (output_state.usage == ValueUsage::Used && !output_state.has_been_computed) {
          required_uncomputed_output_exists = true;
        }
      }
      if (!required_uncomputed_output_exists) {
        return;
      }

      if (node_state.had_initialization) {
        /* Initialize storage. */
        node_state.storage = fn.init_storage(allocator);

        /* Load unlinked inputs. */
        for (const int input_index : node.inputs().index_range()) {
          const LFInputSocket &input_socket = node.input(input_index);
          if (input_socket.origin() != nullptr) {
            continue;
          }
          InputState &input_state = node_state.inputs[input_index];
          const CPPType &type = input_socket.type();
          const void *default_value = input_socket.default_value();
          BLI_assert(default_value != nullptr);
          void *buffer = allocator.allocate(type.size(), type.alignment());
          type.copy_construct(default_value, buffer);
          this->forward_value_to_input(locked_node, input_state, {type, buffer});
        }

        /* Request linked inputs that are always needed. */
        const Span<LazyFunctionInput> fn_inputs = fn.inputs();
        for (const int input_index : fn_inputs.index_range()) {
          const LazyFunctionInput &fn_input = fn_inputs[input_index];
          if (fn_input.usage == ValueUsage::Used) {
            const LFInputSocket &input_socket = node.input(input_index);
            this->set_input_required(locked_node, input_socket);
          }
        }

        node_state.had_initialization = false;
      }

      for (const int input_index : node_state.inputs.index_range()) {
        InputState &input_state = node_state.inputs[input_index];
        if (input_state.was_ready_for_execution) {
          continue;
        }
        if (input_state.value != nullptr) {
          input_state.was_ready_for_execution = true;
          continue;
        }
        if (input_state.usage == ValueUsage::Used) {
          return;
        }
      }

      node_needs_execution = true;
    });

    if (node_needs_execution) {
      this->execute_node(node, node_state, current_task);
    }

    this->with_locked_node(node, node_state, current_task, [&](LockedNode &locked_node) {
      this->finish_node_if_possible(locked_node);
      const bool reschedule_requested = node_state.schedule_state ==
                                        NodeScheduleState::RunningAndRescheduled;
      node_state.schedule_state = NodeScheduleState::NotScheduled;
      if (reschedule_requested && !node_state.node_has_finished) {
        this->schedule_node(locked_node);
      }
#ifdef DEBUG
      if (node_needs_execution) {
        this->assert_expected_outputs_have_been_computed(locked_node);
      }
#endif
    });
  }

  void assert_expected_outputs_have_been_computed(LockedNode &locked_node)
  {
    const NodeState &node_state = locked_node.node_state;
    if (node_state.missing_required_inputs > 0) {
      return;
    }
    if (node_state.schedule_state == NodeScheduleState::Scheduled) {
      return;
    }
    for (const OutputState &output_state : node_state.outputs) {
      if (output_state.usage_for_execution == ValueUsage::Used) {
        BLI_assert(output_state.has_been_computed);
      }
    }
  }

  void finish_node_if_possible(LockedNode &locked_node)
  {
    const LFNode &node = locked_node.node;
    NodeState &node_state = locked_node.node_state;

    if (node_state.node_has_finished) {
      return;
    }
    for (const OutputState &output_state : node_state.outputs) {
      if (output_state.usage != ValueUsage::Unused && !output_state.has_been_computed) {
        return;
      }
    }
    for (const InputState &input_state : node_state.inputs) {
      if (input_state.usage == ValueUsage::Used && !input_state.was_ready_for_execution) {
        return;
      }
    }

    node_state.node_has_finished = true;

    for (const int input_index : node_state.inputs.index_range()) {
      const LFInputSocket &input_socket = node.input(input_index);
      InputState &input_state = node_state.inputs[input_index];
      if (input_state.usage == ValueUsage::Maybe) {
        this->set_input_unused(locked_node, input_socket);
      }
      else if (input_state.usage == ValueUsage::Used) {
        this->destruct_input_value_if_exists(input_state, input_socket.type());
      }
    }

    if (node_state.storage != nullptr) {
      if (node.is_function()) {
        const LFFunctionNode &fn_node = static_cast<const LFFunctionNode &>(node);
        fn_node.function().destruct_storage(node_state.storage);
      }
      node_state.storage = nullptr;
    }
  }

  void destruct_input_value_if_exists(InputState &input_state, const CPPType &type)
  {
    if (input_state.value != nullptr) {
      type.destruct(input_state.value);
      input_state.value = nullptr;
    }
  }

  void execute_node(const LFFunctionNode &node, NodeState &node_state, CurrentTask &current_task);

  void set_input_unused_during_execution(const LFNode &node,
                                         NodeState &node_state,
                                         const int input_index,
                                         CurrentTask &current_task)
  {
    const LFInputSocket &input_socket = node.input(input_index);
    this->with_locked_node(node, node_state, current_task, [&](LockedNode &locked_node) {
      this->set_input_unused(locked_node, input_socket);
    });
  }

  void set_input_unused(LockedNode &locked_node, const LFInputSocket &input_socket)
  {
    NodeState &node_state = locked_node.node_state;
    const int input_index = input_socket.index_in_node();
    InputState &input_state = node_state.inputs[input_index];

    BLI_assert(input_state.usage != ValueUsage::Used);
    if (input_state.usage == ValueUsage::Unused) {
      return;
    }
    input_state.usage = ValueUsage::Unused;

    this->destruct_input_value_if_exists(input_state, input_socket.type());
    if (input_state.was_ready_for_execution) {
      return;
    }
    const LFOutputSocket *origin = input_socket.origin();
    if (origin != nullptr) {
      locked_node.delayed_unused_outputs.append(origin);
    }
  }

  void *set_input_required_during_execution(const LFNode &node,
                                            NodeState &node_state,
                                            const int input_index,
                                            CurrentTask &current_task)
  {
    const LFInputSocket &input_socket = node.input(input_index);
    void *result;
    this->with_locked_node(node, node_state, current_task, [&](LockedNode &locked_node) {
      result = this->set_input_required(locked_node, input_socket);
    });
    return result;
  }

  void *set_input_required(LockedNode &locked_node, const LFInputSocket &input_socket)
  {
    BLI_assert(&locked_node.node == &input_socket.node());
    NodeState &node_state = locked_node.node_state;
    const int input_index = input_socket.index_in_node();
    InputState &input_state = node_state.inputs[input_index];

    BLI_assert(input_state.usage != ValueUsage::Unused);

    if (input_state.value != nullptr) {
      input_state.was_ready_for_execution = true;
      return input_state.value;
    }
    if (input_state.usage == ValueUsage::Used) {
      return nullptr;
    }
    input_state.usage = ValueUsage::Used;
    node_state.missing_required_inputs += 1;

    const LFOutputSocket *origin_socket = input_socket.origin();
    /* Unlinked inputs are always loaded in advance. */
    BLI_assert(origin_socket != nullptr);
    locked_node.delayed_required_outputs.append(origin_socket);
    return nullptr;
  }

  void forward_value_to_linked_inputs(const LFOutputSocket &from_socket,
                                      GMutablePointer value_to_forward,
                                      CurrentTask &current_task)
  {
    BLI_assert(value_to_forward.get() != nullptr);
    LinearAllocator<> &allocator = local_allocators_.local();
    const CPPType &type = *value_to_forward.type();

    const Span<const LFInputSocket *> targets = from_socket.targets();
    for (const LFInputSocket *target_socket : targets) {
      const LFNode &target_node = target_socket->node();
      NodeState &node_state = *node_states_[target_node.index_in_graph()];
      const int input_index = target_socket->index_in_node();
      InputState &input_state = node_state.inputs[input_index];
      const bool is_last_target = target_socket == targets.last();
      BLI_assert(input_state.value == nullptr);
      BLI_assert(!input_state.was_ready_for_execution);
      BLI_assert(target_socket->type() == type);
      BLI_assert(target_socket->origin() == &from_socket);

      if (target_node.is_dummy()) {
        const int graph_output_index = graph_outputs_.index_of_try(target_socket);
        if (graph_output_index != -1 &&
            params_->get_output_usage(graph_output_index) != ValueUsage::Unused) {
          void *dst_buffer = params_->get_output_data_ptr(graph_output_index);
          if (is_last_target) {
            type.move_construct(value_to_forward.get(), dst_buffer);
          }
          else {
            type.copy_construct(value_to_forward.get(), dst_buffer);
          }
          params_->output_set(graph_output_index);
        }
        continue;
      }
      this->with_locked_node(target_node, node_state, current_task, [&](LockedNode &locked_node) {
        if (input_state.usage == ValueUsage::Unused) {
          return;
        }
        if (is_last_target) {
          /* No need to make a copy if this is the last target. */
          this->forward_value_to_input(locked_node, input_state, value_to_forward);
          value_to_forward = {};
        }
        else {
          void *buffer = allocator.allocate(type.size(), type.alignment());
          type.copy_construct(value_to_forward.get(), buffer);
          this->forward_value_to_input(locked_node, input_state, {type, buffer});
        }
      });
    }
    if (value_to_forward.get() != nullptr) {
      value_to_forward.destruct();
    }
  }

  void forward_value_to_input(LockedNode &locked_node,
                              InputState &input_state,
                              GMutablePointer value)
  {
    NodeState &node_state = locked_node.node_state;

    BLI_assert(input_state.value == nullptr);
    BLI_assert(!input_state.was_ready_for_execution);
    input_state.value = value.get();

    if (input_state.usage == ValueUsage::Used) {
      node_state.missing_required_inputs -= 1;
      if (node_state.missing_required_inputs == 0) {
        this->schedule_node(locked_node);
      }
    }
  }
};

class GraphExecutorLazyFunctionParams final : public LazyFunctionParams {
 private:
  Executor &executor_;
  const LFNode &node_;
  NodeState &node_state_;
  CurrentTask &current_task_;

 public:
  GraphExecutorLazyFunctionParams(const LazyFunction &fn,
                                  Executor &executor,
                                  const LFNode &node,
                                  NodeState &node_state,
                                  CurrentTask &current_task)
      : LazyFunctionParams(fn, node_state.storage, executor.params_->user_data()),
        executor_(executor),
        node_(node),
        node_state_(node_state),
        current_task_(current_task)
  {
  }

 private:
  void *try_get_input_data_ptr_impl(const int index) const override
  {
    const InputState &input_state = node_state_.inputs[index];
    if (input_state.was_ready_for_execution) {
      return input_state.value;
    }
    return nullptr;
  }

  void *try_get_input_data_ptr_or_request_impl(const int index) override
  {
    const InputState &input_state = node_state_.inputs[index];
    if (input_state.was_ready_for_execution) {
      return input_state.value;
    }
    return executor_.set_input_required_during_execution(node_, node_state_, index, current_task_);
  }

  void *get_output_data_ptr_impl(const int index) override
  {
    OutputState &output_state = node_state_.outputs[index];
    BLI_assert(!output_state.has_been_computed);
    if (output_state.value == nullptr) {
      LinearAllocator<> &allocator = executor_.local_allocators_.local();
      const CPPType &type = node_.output(index).type();
      output_state.value = allocator.allocate(type.size(), type.alignment());
    }
    return output_state.value;
  }

  void output_set_impl(const int index) override
  {
    OutputState &output_state = node_state_.outputs[index];
    BLI_assert(!output_state.has_been_computed);
    BLI_assert(output_state.value != nullptr);
    const LFOutputSocket &output_socket = node_.output(index);
    executor_.forward_value_to_linked_inputs(
        output_socket, {output_socket.type(), output_state.value}, current_task_);
    output_state.value = nullptr;
    output_state.has_been_computed = true;
  }

  bool output_was_set_impl(const int index) const override
  {
    const OutputState &output_state = node_state_.outputs[index];
    return output_state.has_been_computed;
  }

  ValueUsage get_output_usage_impl(const int index) const override
  {
    const OutputState &output_state = node_state_.outputs[index];
    return output_state.usage_for_execution;
  }

  void set_input_unused_impl(const int index) override
  {
    executor_.set_input_unused_during_execution(node_, node_state_, index, current_task_);
  }
};

void Executor::execute_node(const LFFunctionNode &node,
                            NodeState &node_state,
                            CurrentTask &current_task)
{
  const LazyFunction &fn = node.function();
  GraphExecutorLazyFunctionParams node_params{fn, *this, node, node_state, current_task};
  fn.execute(node_params);
}

LazyFunctionGraphExecutor::LazyFunctionGraphExecutor(const LazyFunctionGraph &graph,
                                                     Span<const LFOutputSocket *> graph_inputs,
                                                     Span<const LFInputSocket *> graph_outputs)
    : graph_(graph), graph_inputs_(graph_inputs), graph_outputs_(graph_outputs)
{
  for (const LFOutputSocket *socket : graph_inputs_) {
    BLI_assert(socket->node().is_dummy());
    inputs_.append({"In", socket->type(), ValueUsage::Maybe});
  }
  for (const LFInputSocket *socket : graph_outputs_) {
    BLI_assert(socket->node().is_dummy());
    outputs_.append({"Out", socket->type()});
  }
}

void LazyFunctionGraphExecutor::execute_impl(LazyFunctionParams &params) const
{
  Executor &executor = params.storage<Executor>();
  executor.execute(params);
}

void *LazyFunctionGraphExecutor::init_storage(LinearAllocator<> &allocator) const
{
  Executor &executor =
      *allocator.construct<Executor>(graph_, graph_inputs_, graph_outputs_).release();
  return &executor;
}

void LazyFunctionGraphExecutor::destruct_storage(void *storage) const
{
  std::destroy_at(static_cast<Executor *>(storage));
}

}  // namespace blender::fn
