/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <mutex>

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_task.h"
#include "BLI_task.hh"

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
};

struct NodeState {
  mutable std::mutex mutex;
  MutableSpan<InputState> inputs;
  MutableSpan<OutputState> outputs;

  int missing_required_inputs = 0;
  bool node_has_finished = false;
  bool always_required_inputs_handled = false;
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
  LazyFunctionParams *params_ = nullptr;
  Map<const LFNode *, NodeState *> node_states_;

  TaskPool *task_pool_ = nullptr;

  threading::EnumerableThreadSpecific<LinearAllocator<>> local_allocators_;

  friend GraphExecutorLazyFunctionParams;

 public:
  Executor(const LazyFunctionGraph &graph,
           const Span<const LFSocket *> inputs,
           const Span<const LFSocket *> outputs)
      : graph_(graph), inputs_(inputs), outputs_(outputs)
  {
    this->initialize_node_states();
    task_pool_ = BLI_task_pool_create(this, TASK_PRIORITY_HIGH);
  }

  ~Executor()
  {
    BLI_task_pool_free(task_pool_);
    for (NodeState *node_state : node_states_.values()) {
      std::destroy_at(node_state);
    }
  }

  void execute(LazyFunctionParams &params)
  {
    params_ = &params;
    this->schedule_newly_requested_outputs();
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
      const LFSocket &socket = *inputs_[io_output_index];
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

  void schedule_newly_requested_outputs()
  {
    for (const int i : outputs_.index_range()) {
      if (params_->get_output_usage(i) != ValueUsage::Used) {
        continue;
      }
      const LFSocket &socket = *outputs_[i];
      const LFNode &node = socket.node();
      NodeState &node_state = *node_states_.lookup(&node);
      UNUSED_VARS(node_state);

      if (socket.is_input()) {
        const LFInputSocket &input_socket = socket.as_input();
        UNUSED_VARS(input_socket);
        /* TODO */
      }
      else {
        const LFOutputSocket &output_socket = socket.as_output();
        UNUSED_VARS(output_socket);
        /* TODO */
      }
    }
  }

  void forward_newly_provided_inputs()
  {
    /* TODO */
  }

  void notify_output_required(const LFOutputSocket &socket)
  {
    /* TODO */
    UNUSED_VARS(socket);
  }

  void notify_output_unused(const LFOutputSocket &socket)
  {
    /* TODO */
    UNUSED_VARS(socket);
  }

  void schedule_node(LockedNode &locked_node)
  {
    /* TODO */
    UNUSED_VARS(locked_node);
  }

  template<typename F> void with_locked_node(const LFNode &node, NodeState &node_state, const F &f)
  {
    LockedNode locked_node{node, node_state};
    {
      std::lock_guard lock{node_state.mutex};
      threading::isolate_task([&]() { f(locked_node); });
    }

    /* TODO: Notifications/ */
  }

  void add_node_to_task_pool(const LFNode &node)
  {
    /* TODO */
    UNUSED_VARS(node);
  }

  static void run_node_from_task_pool(TaskPool *task_pool, void *task_data)
  {
    /* TODO */
    UNUSED_VARS(task_pool, task_data);
  }

  void run_node_task(const LFNode &node)
  {
    /* TODO */
    UNUSED_VARS(node);
  }

  void assert_expected_outputs_have_been_computed(LockedNode &locked_node)
  {
    /* TODO */
    UNUSED_VARS(locked_node);
  }

  void finish_node_if_possible(LockedNode &locked_node)
  {
    /* TODO */
    UNUSED_VARS(locked_node);
  }

  void destruct_input_value_if_exists(LockedNode &locked_node, const LFInputSocket &socket)
  {
    /* TODO */
    UNUSED_VARS(locked_node, socket);
  }

  void execute_node(const LFNode &node, NodeState &node_state);

  void set_input_unused_during_execution(const LFNode &node,
                                         NodeState &node_state,
                                         const int input_index)
  {
    /* TODO */
    UNUSED_VARS(node, node_state, input_index);
  }

  void set_input_unused(LockedNode &locked_node, const LFInputSocket &input_socket)
  {
    /* TODO */
    UNUSED_VARS(locked_node, input_socket);
  }

  void *set_input_required_during_execution(const LFNode &node,
                                            NodeState &node_state,
                                            const int input_index)
  {
    /* TODO */
    UNUSED_VARS(node, node_state, input_index);
    return nullptr;
  }

  void *set_input_required(LockedNode &locked_node, const LFInputSocket &input_socket)
  {
    /* TODO */
    UNUSED_VARS(locked_node, input_socket);
    return nullptr;
  }

  void forward_output_provided_by_outside(const LFOutputSocket &from_socket,
                                          GMutablePointer value_to_forward)
  {
    /* TODO */
    UNUSED_VARS(from_socket, value_to_forward);
  }

  void forward_computed_node_output(const LFOutputSocket &from_socket,
                                    GMutablePointer value_to_forward)
  {
    /* TODO */
    UNUSED_VARS(from_socket, value_to_forward);
  }

  void forward_value_to_linked_inputs(const LFOutputSocket &from_socket,
                                      GMutablePointer value_to_forward)
  {
    /* TODO */
    UNUSED_VARS(from_socket, value_to_forward);
  }

  void forward_value_to_input(const LFInputSocket &to_socket, GMutablePointer value)
  {
    /* TODO */
    UNUSED_VARS(to_socket, value);
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
  void *try_get_input_data_ptr_impl(int index) override
  {
    /* TODO */
    UNUSED_VARS(index);
    return nullptr;
  }

  void *get_output_data_ptr_impl(int index) override
  {
    /* TODO */
    UNUSED_VARS(index);
    return nullptr;
  }

  void output_set_impl(int index) override
  {
    /* TODO */
    UNUSED_VARS(index);
  }

  ValueUsage get_output_usage_impl(int index) override
  {
    /* TODO */
    UNUSED_VARS(index);
    return ValueUsage::Used;
  }

  void set_input_unused_impl(int index) override
  {
    /* TODO */
    UNUSED_VARS(index);
  }
};

void Executor::execute_node(const LFNode &node, NodeState &node_state)
{
  /* TODO */
  UNUSED_VARS(node, node_state);
}

LazyFunctionGraphExecutor::LazyFunctionGraphExecutor(const LazyFunctionGraph &graph,
                                                     Vector<const LFSocket *> inputs,
                                                     Vector<const LFSocket *> outputs)
    : graph_(graph), input_sockets_(std::move(inputs)), output_sockets_(std::move(outputs))
{
  for (const LFSocket *socket : input_sockets_) {
    inputs_.append({"In", socket->type()});
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
