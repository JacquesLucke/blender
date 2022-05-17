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

class Executor {
 private:
  const LazyFunctionGraph &graph_;
  Span<const LFSocket *> inputs_;
  Span<const LFSocket *> outputs_;
  LazyFunctionParams *params_ = nullptr;
  Map<const LFNode *, NodeState *> node_states_;

  threading::EnumerableThreadSpecific<LinearAllocator<>> local_allocators_;

 public:
  Executor(const LazyFunctionGraph &graph,
           const Span<const LFSocket *> inputs,
           const Span<const LFSocket *> outputs)
      : graph_(graph), inputs_(inputs), outputs_(outputs)
  {
    this->initialize_node_states();
  }

  void execute(LazyFunctionParams &params)
  {
    params_ = &params;
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
};

LazyFunctionGraphExecutor::LazyFunctionGraphExecutor(const LazyFunctionGraph &graph,
                                                     Vector<const LFSocket *> inputs,
                                                     Vector<const LFSocket *> outputs)
    : graph_(graph), inputs_(std::move(inputs)), outputs_(std::move(outputs))
{
}

void LazyFunctionGraphExecutor::execute_impl(LazyFunctionParams &params) const
{
  Executor &executor = params.storage<Executor>();
  executor.execute(params);
}

void *LazyFunctionGraphExecutor::init_storage(LinearAllocator<> &allocator) const
{
  Executor &executor = *allocator.construct<Executor>(graph_, inputs_, outputs_).release();
  return &executor;
}

void LazyFunctionGraphExecutor::destruct_storage(void *storage) const
{
  std::destroy_at(static_cast<Executor *>(storage));
}

}  // namespace blender::fn
