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

#include "BLI_set.hh"
#include "BLI_stack.hh"

namespace blender::modifiers {

namespace {
struct OutputSocketState {
  Vector<DInputSocket> users;
};

struct NodeState {
  MutableSpan<OutputSocketState> outputs;
};

class Evaluator {
 private:
  GeometryNodesEvaluationParams &params_;
  LinearAllocator<> &allocator_;
  Map<DNode, NodeState> node_states_;

 public:
  Evaluator(GeometryNodesEvaluationParams &params) : params_(params), allocator_(params_.allocator)
  {
  }

  void execute()
  {
    this->initialize_node_states();
    this->initialize_socket_users();
    node_states_.print_stats();
    this->destruct_node_states();
  }

  void initialize_node_states()
  {
    SCOPED_TIMER(__func__);
    Stack<DNode> nodes_to_check;
    for (const DInputSocket &socket : params_.output_sockets) {
      nodes_to_check.push(socket.node());
    }
    while (!nodes_to_check.is_empty()) {
      DNode node = nodes_to_check.pop();
      bool newly_created = false;
      node_states_.lookup_or_add_cb(node, [&, this]() {
        newly_created = true;
        NodeState state;
        state.outputs = this->allocator_.construct_array<OutputSocketState>(
            node->outputs().size());
        return state;
      });
      if (newly_created) {
        for (const InputSocketRef *input_socket_ref : node->inputs()) {
          DInputSocket input_socket{node.context(), input_socket_ref};
          input_socket.foreach_origin_socket(
              [&](const DSocket origin_socket) { nodes_to_check.push(origin_socket.node()); });
        }
      }
    }
  }

  void destruct_node_states()
  {
    SCOPED_TIMER(__func__);
    for (NodeState &state : node_states_.values()) {
      destruct_n(state.outputs.data(), state.outputs.size());
    }
  }

  void initialize_socket_users()
  {
    SCOPED_TIMER(__func__);
    for (auto &&item : node_states_.items()) {
      const DNode node = item.key;
      NodeState &node_state = item.value;
      for (const int output_index : node->outputs().index_range()) {
        const DOutputSocket output_socket{node.context(), &node->output(output_index)};
        output_socket.foreach_target_socket(
            [&, this](const DInputSocket target_socket) {
              const DNode target_node = target_socket.node();
              if (this->node_states_.contains(target_node)) {
                node_state.outputs[output_index].users.append_non_duplicates(target_socket);
              }
            },
            {});
      }
    }
  }
};
}  // namespace

void evaluate_geometry_nodes(GeometryNodesEvaluationParams &params)
{
  Evaluator evaluator{params};
  evaluator.execute();
}

}  // namespace blender::modifiers
