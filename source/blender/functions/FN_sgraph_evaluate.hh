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

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_stack.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

#include "FN_cpp_type.hh"
#include "FN_generic_pointer.hh"
#include "FN_sgraph.hh"

namespace blender::fn::sgraph {

enum class LazyRequireInputResult {
  Ready,
  NotYetAvailable,
};

class ExecuteNodeParams {
 public:
  bool is_input_available(int index) const;
  bool output_was_set(int index) const;

  GMutablePointer extract_input(int index);
  GPointer get_input(int index) const;

  void set_output_by_copy(int index, GPointer value);
  void set_output_by_move(int index, GMutablePointer value);

  bool output_is_required(int index) const;

  LazyRequireInputResult lazy_require_input(int index);
  bool lazy_output_is_required(int index);
};

class ExecuteGraphParams {
  LazyRequireInputResult require_input(int index);
  void load_input(int index, GMutablePointer r_value);
  bool output_is_required(int index) const;
  void set_output_by_move(int index, GMutablePointer value);
};

enum class ValueUsage : uint8_t {
  Required,
  Maybe,
  Unused,
};

struct SingleInputValue {
  void *value = nullptr;
};

struct MultiInputValue {
  Vector<void *> values;
  int provided_value_count = 0;

  int missing_values() const
  {
    return this->values.size() - this->provided_value_count;
  }
};

struct InputState {
  const CPPType *type = nullptr;

  union {
    SingleInputValue *single;
    MultiInputValue *multi;
  } value;

  ValueUsage usage = ValueUsage::Maybe;
  bool was_ready_for_execution = false;
};

struct OutputState {
  const CPPType *type = nullptr;
  ValueUsage usage = ValueUsage::Maybe;
  int potential_users = 0;
  bool has_been_computed = false;
};

struct NodeState {
  std::mutex mutex;
  MutableSpan<InputState> inputs;
  MutableSpan<OutputState> outputs;

  int missing_required_inputs = 0;
};

template<typename SGraphAdapter, typename Executor> class SGraphEvaluator {
 private:
  using SGraph = SGraphT<SGraphAdapter>;
  using Node = NodeT<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;
  using Socket = SocketT<SGraphAdapter>;
  using Link = LinkT<SGraphAdapter>;

  LinearAllocator<> allocator_;
  const SGraph graph_;
  Executor executor_;
  const VectorSet<Socket> input_sockets_;
  const VectorSet<Socket> output_sockets_;
  Map<Node, destruct_ptr<NodeState>> node_states_;
  TaskPool *task_pool_ = nullptr;

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
                  Executor executor,
                  const Span<Socket> input_sockets,
                  const Span<Socket> output_sockets)
      : graph_(std::move(graph)),
        executor_(std::move(executor)),
        input_sockets_(input_sockets),
        output_sockets_(output_sockets)
  {
    this->initialize_reachable_node_states();
    task_pool_ = BLI_task_pool_create(this, TASK_PRIORITY_HIGH);
  }

  ~SGraphEvaluator()
  {
    BLI_task_pool_free(task_pool_);
  }

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
      node_state.outputs = allocator_.construct_array<OutputState>(node.inputs_size(graph_));

      for (const int input_index : node_state.inputs.index_range()) {
        InputState &input_state = node_state.inputs[input_index];
        input_state.type = executor_.input_socket_type(node.id, input_index);
        if (input_state.type == nullptr) {
          input_state.usage = ValueUsage::Unused;
        }
      }
      for (const int output_index : node_state.outputs.index_range()) {
        OutSocket out_socket{node, output_index};
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
  }

  void execute(ExecuteGraphParams &params)
  {
    const Vector<Socket> sockets_to_compute = this->find_sockets_to_compute(params);
    this->schedule_initial_nodes(sockets_to_compute);
  }

 private:
  Vector<Socket> find_sockets_to_compute(ExecuteGraphParams &params) const
  {
    Vector<Socket> sockets_to_compute;
    for (const int i : output_sockets_.index_range()) {
      if (!params.output_is_required(i)) {
        continue;
      }
      const Socket socket = output_sockets_[i];
      const NodeState &node_state = node_states_.lookup(socket.node);
      const OutputState &output_state = node_state.outputs[socket.index];
      if (output_state.has_been_computed) {
        continue;
      }
      sockets_to_compute.append(socket);
    }
    return sockets_to_compute;
  }

  void schedule_initial_nodes(const Span<Socket> sockets_to_compute) const
  {
    for (const Socket &socket : sockets_to_compute) {
      const Node node = socket.node;
      NodeState &node_state = node_states_.lookup(node);
      if (socket.is_input) {
        /* TODO */
      }
      else {
        /* TODO */
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
    evaluator.execute_node(node);
  }

  void execute_node(const Node &node)
  {
    std::cout << "Execute node: " << node.id << "\n";
  }

  LazyRequireInputResult lazy_require_input(LockedNode &locked_node, const InSocket in_socket)
  {
    BLI_assert(locked_node.node == in_socket.node);
    InputState &input_state = locked_node.node_state.inputs[in_socket.index];

    BLI_assert(input_state.usage != ValueUsage::Unused);

    if (input_state.was_ready_for_execution) {
      return LazyRequireInputResult::Ready;
    }

    if (input_state.usage == ValueUsage::Required) {
      return LazyRequireInputResult::NotYetAvailable;
    }
    input_state.usage = ValueUsage::Required;

    int missing_values = 0;
    if (executor_.is_multi_input(locked_node.node, in_socket.index)) {
      MultiInputValue &multi_value = *input_state.value.multi;
      missing_values = multi_value.missing_values();
    }
    else {
      SingleInputValue &single_value = *input_state.value.single;
      if (single_value.value == nullptr) {
        missing_values = 1;
      }
    }
    BLI_assert(missing_values > 0);
    locked_node.node_state.missing_required_inputs += missing_values;

    Vector<OutSocket> origin_sockets;
    in_socket.foreach_linked(
        graph_, [&](const OutSocket &origin_socket) { origin_sockets.append(origin_socket); });

    if (origin_sockets.is_empty()) {
      this->load_unlinked_input_value(locked_node, in_socket);
      locked_node.node_state.missing_required_inputs -= 1;
      input_state.was_ready_for_execution = true;
      return LazyRequireInputResult::Ready;
    }

    for (const OutSocket &origin_socket : origin_sockets) {
      locked_node.delayed_required_outputs.append(origin_socket);
    }

    return LazyRequireInputResult::NotYetAvailable;
  }

  void load_unlinked_input_value(LockedNode &locked_node, const InSocket in_socket)
  {
  }
};

}  // namespace blender::fn::sgraph
