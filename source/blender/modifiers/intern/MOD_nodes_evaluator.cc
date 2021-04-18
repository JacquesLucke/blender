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

    const DerivedNodeTree &tree = params_.output_sockets[0].context()->derived_tree();
    tree.foreach_node([&, this](const DNode node) {
      if (node->name() == "Join Geometry") {
        this->disable_input_socket(DInputSocket{node.context(), &node->input(0)});
      }
    });

    auto get_socket_users = [&, this](const DSocket socket) -> std::string {
      if (socket->is_input()) {
        return "";
      }
      const DOutputSocket output_socket{socket};
      NodeState *node_state = this->node_states_.lookup_ptr(output_socket.node());
      if (node_state == nullptr) {
        return " 0";
      }
      return " " + std::to_string(node_state->outputs[socket->index()].users.size());
    };

    DerivedNodeTree::ToDotParams to_dot_params;
    to_dot_params.get_additional_socket_label = get_socket_users;
    std::cout << "\n\n" << tree.to_dot(to_dot_params) << "\n\n";

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

  void disable_input_socket(const DInputSocket socket_to_disable)
  {
    Stack<DNode> nodes_to_check;
    this->disable_input_socket_direct(socket_to_disable, nodes_to_check);

    while (!nodes_to_check.is_empty()) {
      const DNode node = nodes_to_check.pop();
      NodeState &node_state = node_states_.lookup(node);
      bool any_output_has_users = false;
      for (const OutputSocketState &socket_state : node_state.outputs) {
        if (!socket_state.users.is_empty()) {
          any_output_has_users = true;
        }
      }
      if (any_output_has_users) {
        continue;
      }
      for (const InputSocketRef *input_socket_ref : node->inputs()) {
        const DInputSocket input_socket{node.context(), input_socket_ref};
        this->disable_input_socket_direct(input_socket, nodes_to_check);
      }
    }
  }

  void disable_input_socket_direct(const DInputSocket socket_to_disable,
                                   Stack<DNode> &nodes_to_check)
  {
    socket_to_disable.foreach_origin_socket([&, this](const DSocket origin_socket) {
      if (origin_socket->is_input()) {
        return;
      }
      const DNode origin_node = origin_socket.node();
      NodeState &node_state = this->node_states_.lookup(origin_node);
      OutputSocketState &socket_state = node_state.outputs[origin_socket->index()];
      const int index_to_remove = socket_state.users.first_index_of_try(socket_to_disable);
      if (index_to_remove == -1) {
        /* Has been removed before. */
        return;
      }
      socket_state.users.remove_and_reorder(index_to_remove);
      nodes_to_check.push(origin_node);
    });
  }
};
}  // namespace

void evaluate_geometry_nodes(GeometryNodesEvaluationParams &params)
{
  Evaluator evaluator{params};
  evaluator.execute();
}

}  // namespace blender::modifiers
