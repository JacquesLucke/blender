/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <mutex>

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "FN_lazy_function_graph_executor.hh"

namespace blender::fn {

enum class NodeScheduleState {
  NotScheduled,
  Scheduled,
  Running,
  RunningAndRescheduled,
};

struct IOIndices {
  int input_index = -1;
  int output_index = -1;
};

struct InputState {
  const CPPType *type = nullptr;
  void *value = nullptr;
  ValueUsage usage = ValueUsage::Maybe;
  bool was_ready_for_execution = false;
  bool is_destructed = false;
  IOIndices io;
};

struct OutputState {
  const CPPType *type = nullptr;
  ValueUsage usage = ValueUsage::Maybe;
  ValueUsage usage_for_execution = ValueUsage::Maybe;
  int potential_target_sockets = 0;
  bool has_been_computed = false;
  IOIndices io;
  void *value = nullptr;
};

struct NodeState {
  mutable std::mutex mutex;
  MutableSpan<InputState> inputs;
  MutableSpan<OutputState> outputs;

  int missing_required_inputs = 0;
  bool node_has_finished = false;
  bool default_inputs_initialized = false;
  bool always_required_inputs_handled = false;
  bool storage_initialized = false;
  NodeScheduleState schedule_state = NodeScheduleState::NotScheduled;
  void *storage = nullptr;
};

struct LockedNode {
  const LFNode &node;
  NodeState &node_state;

  Vector<const LFOutputSocket *> delayed_required_outputs;
  Vector<const LFOutputSocket *> delayed_unused_outputs;
  Vector<const LFNode *> delayed_scheduled_nodes;

  LockedNode(const LFNode &node, NodeState &node_state) : node(node), node_state(node_state)
  {
  }
};

class GraphExecutorLazyFunctionParams;

class Executor {
 private:
  const LazyFunctionGraph &graph_;
  Span<const LFSocket *> inputs_;
  Span<const LFSocket *> outputs_;
  Array<bool> loaded_inputs_;
  LazyFunctionParams *params_ = nullptr;
  Map<const LFNode *, NodeState *> node_states_;

  TaskPool *task_pool_ = nullptr;

  threading::EnumerableThreadSpecific<LinearAllocator<>> local_allocators_;

  friend GraphExecutorLazyFunctionParams;

 public:
  Executor(const LazyFunctionGraph &graph,
           const Span<const LFSocket *> inputs,
           const Span<const LFSocket *> outputs)
      : graph_(graph), inputs_(inputs), outputs_(outputs), loaded_inputs_(inputs.size(), false)
  {
    this->initialize_node_states();
    task_pool_ = BLI_task_pool_create(this, TASK_PRIORITY_HIGH);
  }

  ~Executor()
  {
    BLI_task_pool_free(task_pool_);
    for (auto item : node_states_.items()) {
      this->destruct_node_state(*item.key, *item.value);
    }
  }

  void execute(LazyFunctionParams &params)
  {
    params_ = &params;
    this->schedule_newly_requested_outputs();
    this->forward_newly_provided_inputs();
    BLI_task_pool_work_and_wait(task_pool_);
    params_ = nullptr;
  }

 private:
  void initialize_node_states()
  {
    Span<const LFNode *> nodes = graph_.nodes();
    MutableSpan<NodeState> node_states_span = local_allocators_.local().allocate_array<NodeState>(
        nodes.size());

    threading::parallel_invoke(
        [&]() {
          for (const int i : nodes.index_range()) {
            node_states_.add_new(nodes[i], &node_states_span[i]);
          }
        },
        [&]() {
          threading::parallel_for(nodes.index_range(), 256, [&](const IndexRange range) {
            LinearAllocator<> &allocator = local_allocators_.local();
            for (const int i : range) {
              NodeState &node_state = node_states_span[i];
              new (&node_state) NodeState();
              this->construct_initial_node_state(allocator, *nodes[i], node_state);
            }
          });
        });

    for (const int io_input_index : inputs_.index_range()) {
      const LFSocket &socket = *inputs_[io_input_index];
      const LFNode &node = socket.node();
      NodeState &node_state = *node_states_.lookup(&node);
      const int index_in_node = socket.index_in_node();
      if (socket.is_input()) {
        node_state.inputs[index_in_node].io.input_index = io_input_index;
      }
      else {
        node_state.outputs[index_in_node].io.input_index = io_input_index;
      }
    }
    for (const int io_output_index : outputs_.index_range()) {
      const LFSocket &socket = *outputs_[io_output_index];
      const LFNode &node = socket.node();
      NodeState &node_state = *node_states_.lookup(&node);
      const int index_in_node = socket.index_in_node();
      if (socket.is_input()) {
        node_state.inputs[index_in_node].io.output_index = io_output_index;
      }
      else {
        OutputState &output_state = node_state.outputs[index_in_node];
        output_state.io.output_index = io_output_index;
        /* Update usage, in case it has been set to Unused before. */
        output_state.usage = ValueUsage::Maybe;
      }
    }
  }

  void construct_initial_node_state(LinearAllocator<> &allocator,
                                    const LFNode &node,
                                    NodeState &node_state)
  {
    const Span<const LFInputSocket *> node_inputs = node.inputs();
    const Span<const LFOutputSocket *> node_outputs = node.outputs();

    node_state.inputs = allocator.construct_array<InputState>(node_inputs.size());
    node_state.outputs = allocator.construct_array<OutputState>(node_outputs.size());

    for (const int i : node_inputs.index_range()) {
      InputState &input_state = node_state.inputs[i];
      const LFInputSocket &input_socket = *node_inputs[i];
      input_state.type = &input_socket.type();
    }

    for (const int i : node_outputs.index_range()) {
      OutputState &output_state = node_state.outputs[i];
      const LFOutputSocket &output_socket = *node_outputs[i];
      output_state.type = &output_socket.type();
      output_state.potential_target_sockets = output_socket.targets().size();
      if (output_state.potential_target_sockets == 0) {
        /* Might be changed again, if this is a graph output socket. */
        output_state.usage = ValueUsage::Unused;
      }
    }
  }

  void destruct_node_state(const LFNode &node, NodeState &node_state)
  {
    const LazyFunction &fn = node.function();
    if (node_state.storage != nullptr) {
      fn.destruct_storage(node_state.storage);
    }
    for (InputState &input_state : node_state.inputs) {
      this->destruct_input_value_if_exists(input_state);
    }
    std::destroy_at(&node_state);
  }

  void schedule_newly_requested_outputs()
  {
    for (const int io_output_index : outputs_.index_range()) {
      if (params_->get_output_usage(io_output_index) != ValueUsage::Used) {
        continue;
      }
      const LFSocket &socket = *outputs_[io_output_index];
      const LFNode &node = socket.node();
      NodeState &node_state = *node_states_.lookup(&node);
      UNUSED_VARS(node_state);

      if (socket.is_input()) {
        const LFInputSocket &input_socket = socket.as_input();
        this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
          this->set_input_required(locked_node, input_socket);
        });
      }
      else {
        const LFOutputSocket &output_socket = socket.as_output();
        const int index_in_node = output_socket.index_in_node();
        OutputState &output_state = node_state.outputs[index_in_node];
        if (!output_state.has_been_computed) {
          this->notify_output_required(output_socket);
        }
      }
    }
  }

  void forward_newly_provided_inputs()
  {
    LinearAllocator<> &allocator = local_allocators_.local();
    for (const int io_input_index : inputs_.index_range()) {
      if (loaded_inputs_[io_input_index]) {
        continue;
      }
      void *input_data = params_->try_get_input_data_ptr(io_input_index);
      if (input_data == nullptr) {
        continue;
      }
      const LFSocket &socket = *inputs_[io_input_index];
      const LFNode &node = socket.node();
      NodeState &node_state = *node_states_.lookup(&node);
      this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
        const int index_in_node = socket.index_in_node();
        const CPPType &type = socket.type();
        void *buffer = allocator.allocate(type.size(), type.alignment());
        type.move_construct(input_data, buffer);
        if (socket.is_input()) {
          InputState &input_state = node_state.inputs[index_in_node];
          this->forward_value_to_input(locked_node, input_state, {type, buffer});
        }
        else {
          const LFOutputSocket &output_socket = socket.as_output();
          OutputState &output_state = node_state.outputs[index_in_node];
          this->forward_output_provided_by_outside(output_state, output_socket, {type, buffer});
        }
      });
    }
  }

  void notify_output_required(const LFOutputSocket &socket)
  {
    const LFNode &node = socket.node();
    const int index_in_node = socket.index_in_node();
    NodeState &node_state = *node_states_.lookup(&node);
    OutputState &output_state = node_state.outputs[index_in_node];

    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      if (output_state.usage == ValueUsage::Used) {
        return;
      }
      output_state.usage = ValueUsage::Used;
      this->schedule_node(locked_node);
    });
  }

  void notify_output_unused(const LFOutputSocket &socket)
  {
    const LFNode &node = socket.node();
    const int index_in_node = socket.index_in_node();
    NodeState &node_state = *node_states_.lookup(&node);
    OutputState &output_state = node_state.outputs[index_in_node];

    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      output_state.potential_target_sockets -= 1;
      if (output_state.potential_target_sockets == 0) {
        BLI_assert(output_state.usage != ValueUsage::Unused);
        if (output_state.usage == ValueUsage::Maybe && output_state.io.output_index == -1) {
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
        locked_node.delayed_scheduled_nodes.append(&locked_node.node);
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

  template<typename F> void with_locked_node(const LFNode &node, NodeState &node_state, const F &f)
  {
    BLI_assert(&node_state == node_states_.lookup(&node));

    LockedNode locked_node{node, node_state};
    {
      std::lock_guard lock{node_state.mutex};
      threading::isolate_task([&]() { f(locked_node); });
    }

    for (const LFOutputSocket *socket : locked_node.delayed_required_outputs) {
      this->notify_output_required(*socket);
    }
    for (const LFOutputSocket *socket : locked_node.delayed_unused_outputs) {
      this->notify_output_unused(*socket);
    }
    for (const LFNode *node_to_schedule : locked_node.delayed_scheduled_nodes) {
      this->add_node_to_task_pool(*node_to_schedule);
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
    const LFNode &node = *static_cast<const LFNode *>(task_data);
    executor.run_node_task(node);
  }

  void run_node_task(const LFNode &node)
  {
    NodeState &node_state = *node_states_.lookup(&node);
    LinearAllocator<> &allocator = local_allocators_.local();

    bool node_needs_execution = false;
    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
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

      if (!node_state.default_inputs_initialized) {
        for (const int input_index : node.inputs().index_range()) {
          const LFInputSocket &input_socket = node.input(input_index);
          if (input_socket.origin() != nullptr) {
            continue;
          }
          InputState &input_state = node_state.inputs[input_index];
          if (input_state.io.input_index != -1) {
            continue;
          }
          const CPPType &type = input_socket.type();
          const void *default_value = input_socket.default_value();
          BLI_assert(default_value != nullptr);
          void *buffer = allocator.allocate(type.size(), type.alignment());
          type.copy_construct(default_value, buffer);
          this->forward_value_to_input(locked_node, input_state, {type, buffer});
        }
        node_state.default_inputs_initialized = true;
      }

      if (!node_state.storage_initialized) {
        node_state.storage = node.function().init_storage(allocator);
        node_state.storage_initialized = true;
      }

      if (!node_state.always_required_inputs_handled) {
        const LazyFunction &fn = node.function();
        const Span<LazyFunctionInput> fn_inputs = fn.inputs();
        for (const int input_index : fn_inputs.index_range()) {
          const LazyFunctionInput &fn_input = fn_inputs[input_index];
          if (fn_input.usage == ValueUsage::Used) {
            const LFInputSocket &input_socket = node.input(input_index);
            this->set_input_required(locked_node, input_socket);
          }
        }
        node_state.always_required_inputs_handled = true;
      }

      for (const int input_index : node_state.inputs.index_range()) {
        InputState &input_state = node_state.inputs[input_index];
        if (input_state.was_ready_for_execution) {
          continue;
        }
        if (input_state.value != nullptr) {
          input_state.was_ready_for_execution = true;
        }
        if (input_state.usage == ValueUsage::Used && !input_state.was_ready_for_execution) {
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
        this->destruct_input_value_if_exists(input_state);
      }
    }

    if (node_state.storage != nullptr) {
      node.function().destruct_storage(node_state.storage);
      node_state.storage = nullptr;
    }
  }

  void destruct_input_value_if_exists(InputState &input_state)
  {
    if (input_state.value != nullptr) {
      const CPPType &type = *input_state.type;
      type.destruct(input_state.value);
      input_state.value = nullptr;
    }
  }

  void execute_node(const LFNode &node, NodeState &node_state);

  void set_input_unused_during_execution(const LFNode &node,
                                         NodeState &node_state,
                                         const int input_index)
  {
    const LFInputSocket &input_socket = node.input(input_index);
    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
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

    this->destruct_input_value_if_exists(input_state);
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
                                            const int input_index)
  {
    const LFInputSocket &input_socket = node.input(input_index);
    void *result;
    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
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

    if (input_state.io.input_index != -1) {
      /* TODO: Can use value from here if it is available already? */
      params_->try_get_input_data_ptr_or_request(input_state.io.input_index);
      return nullptr;
    }

    const LFOutputSocket *origin_socket = input_socket.origin();
    /* Unlinked inputs are always loaded in advance. */
    BLI_assert(origin_socket != nullptr);
    locked_node.delayed_required_outputs.append(origin_socket);
    return nullptr;
  }

  void forward_output_provided_by_outside(OutputState &output_state,
                                          const LFOutputSocket &from_socket,
                                          GMutablePointer value_to_forward)
  {
    this->copy_value_to_graph_output_if_necessary(value_to_forward, output_state.io.output_index);
    this->forward_value_to_linked_inputs(from_socket, value_to_forward);
  }

  void forward_computed_node_output(OutputState &output_state,
                                    const LFOutputSocket &from_socket,
                                    GMutablePointer value_to_forward)
  {
    BLI_assert(value_to_forward.get() != nullptr);

    if (output_state.io.input_index != -1) {
      /* The computed value is ignored, because it is overridden from the outside. */
      value_to_forward.destruct();
      return;
    }
    this->copy_value_to_graph_output_if_necessary(value_to_forward, output_state.io.output_index);
    this->forward_value_to_linked_inputs(from_socket, value_to_forward);
  }

  void copy_value_to_graph_output_if_necessary(GMutablePointer value, const int io_output_index)
  {
    if (io_output_index == -1) {
      return;
    }
    if (params_->get_output_usage(io_output_index) == ValueUsage::Unused) {
      return;
    }
    void *dst_buffer = params_->get_output_data_ptr(io_output_index);
    const CPPType &type = *value.type();
    type.copy_construct(value.get(), dst_buffer);
    params_->output_set(io_output_index);
  }

  void forward_value_to_linked_inputs(const LFOutputSocket &from_socket,
                                      GMutablePointer value_to_forward)
  {
    LinearAllocator<> &allocator = local_allocators_.local();

    const Span<const LFInputSocket *> targets = from_socket.targets();
    for (const LFInputSocket *target_socket : targets) {
      const LFNode &target_node = target_socket->node();
      NodeState &node_state = *node_states_.lookup(&target_node);
      const int input_index = target_socket->index_in_node();
      InputState &input_state = node_state.inputs[input_index];
      BLI_assert(input_state.value == nullptr);
      BLI_assert(!input_state.was_ready_for_execution);

      if (input_state.io.input_index != -1) {
        continue;
      }
      this->with_locked_node(target_node, node_state, [&](LockedNode &locked_node) {
        if (input_state.usage == ValueUsage::Unused) {
          return;
        }
        if (target_socket == targets.last()) {
          /* No need to make a copy if this is the last target. */
          this->forward_value_to_input(locked_node, input_state, value_to_forward);
          value_to_forward = {};
        }
        else {
          const CPPType &type = *value_to_forward.type();
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
    BLI_assert(input_state.type == value.type());
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

 public:
  GraphExecutorLazyFunctionParams(const LazyFunction &fn,
                                  Executor &executor,
                                  const LFNode &node,
                                  NodeState &node_state)
      : LazyFunctionParams(fn, node_state.storage),
        executor_(executor),
        node_(node),
        node_state_(node_state)
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
    return executor_.set_input_required_during_execution(node_, node_state_, index);
  }

  void *get_output_data_ptr_impl(const int index) override
  {
    OutputState &output_state = node_state_.outputs[index];
    BLI_assert(!output_state.has_been_computed);
    if (output_state.value == nullptr) {
      LinearAllocator<> &allocator = executor_.local_allocators_.local();
      const CPPType &type = *output_state.type;
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
    executor_.forward_computed_node_output(
        output_state, output_socket, {output_state.type, output_state.value});
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
    executor_.set_input_unused_during_execution(node_, node_state_, index);
  }
};

void Executor::execute_node(const LFNode &node, NodeState &node_state)
{
  const LazyFunction &fn = node.function();
  GraphExecutorLazyFunctionParams node_params{fn, *this, node, node_state};
  fn.execute(node_params);
}

LazyFunctionGraphExecutor::LazyFunctionGraphExecutor(const LazyFunctionGraph &graph,
                                                     Vector<const LFSocket *> inputs,
                                                     Vector<const LFSocket *> outputs)
    : graph_(graph), input_sockets_(std::move(inputs)), output_sockets_(std::move(outputs))
{
  for (const LFSocket *socket : input_sockets_) {
    inputs_.append({"In", socket->type(), ValueUsage::Maybe});
  }
  for (const LFSocket *socket : output_sockets_) {
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
      *allocator.construct<Executor>(graph_, input_sockets_, output_sockets_).release();
  return &executor;
}

void LazyFunctionGraphExecutor::destruct_storage(void *storage) const
{
  std::destroy_at(static_cast<Executor *>(storage));
}

}  // namespace blender::fn
