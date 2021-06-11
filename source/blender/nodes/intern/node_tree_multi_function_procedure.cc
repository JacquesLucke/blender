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

#include "BLI_stack.hh"

#include "NOD_node_tree_multi_function_procedure.hh"

namespace blender::nodes {

class NodeTreeProcedureBuilder {
 private:
  const DerivedNodeTree &tree_;
  const Span<DSocket> tree_outputs_;
  std::unique_ptr<MFProcedure> procedure_;
  Map<DSocket, MFVariable *> variable_by_socket_;

  friend NodeMFProcedureBuilder;

 public:
  NodeTreeProcedureBuilder(const DerivedNodeTree &tree, const Span<DSocket> tree_outputs)
      : tree_(tree), tree_outputs_(tree_outputs)
  {
    procedure_ = std::make_unique<MFProcedure>();
  }

  MFProcedureFromNodes build()
  {
    Stack<DSocket> sockets_to_compute;
    sockets_to_compute.push_multiple(tree_outputs_);

    while (!sockets_to_compute.is_empty()) {
      const DSocket socket_to_compute = sockets_to_compute.peek();
      if (variable_by_socket_.contains(socket_to_compute)) {
        sockets_to_compute.pop();
        continue;
      }
      const DNode node = socket_to_compute.node();
      bNodeType *node_type = node->typeinfo();
      if (false /* Handle undefined/unsupported nodes. */) {
      }
      if (socket_to_compute->is_input()) {
        const DInputSocket input_socket{socket_to_compute};
        Vector<DSocket> origin_sockets;
        input_socket.foreach_origin_socket(
            [&](const DSocket socket) { origin_sockets.append(socket); });
        if (origin_sockets.is_empty()) {
          /* Load from socket. */
        }
        else if (origin_sockets.size() == 1) {
          /* Compute origin. */
        }
        else {
          /* Handle error. */
        }
      }
      else {
        const DOutputSocket output_socket{socket_to_compute};
        bool all_inputs_computed = true;
        for (const int i : node->inputs().index_range()) {
          const DInputSocket node_input = node.input(i);
          if (node_input->is_available()) {
            if (!variable_by_socket_.contains(node_input)) {
              sockets_to_compute.push(node_input);
              all_inputs_computed = false;
            }
          }
        }
        if (!all_inputs_computed) {
          continue;
        }

        NodeMFProcedureBuilder node_builder{node, *this};
        node_type->build_mf_procedure(node_builder);
      }
    }

    return {std::move(procedure_), {}};
  }
};

void NodeMFProcedureBuilder::set_matching_fn(const MultiFunction &fn)
{
  MFCallInstruction &instruction = procedure_builder_.procedure_->new_call_instruction(fn);
  Vector<MFVariable *> variables;
  for (const int i : node_->inputs().index_range()) {
    const DInputSocket socket = node_.input(i);
    if (socket->is_available()) {
      variables.append(procedure_builder_.variable_by_socket_.lookup(socket));
    }
  }
  for (const int i : node_->outputs().index_range()) {
    const DOutputSocket socket = node_.output(i);
    if (socket->is_available()) {
      const int param_index = variables.size();
      const MFParamType param_type = fn.param_type(param_index);
      const StringRef name = fn.param_name(param_index);
      MFVariable &output_variable = procedure_builder_.procedure_->new_variable(
          param_type.data_type(), name);
      procedure_builder_.variable_by_socket_.add_new(socket, &output_variable);
      variables.append(&output_variable);
    }
  }
  instruction.set_params(variables);
}

MFProcedureFromNodes create_multi_function_procedure(const DerivedNodeTree &tree,
                                                     const Span<DSocket> tree_outputs)
{
  NodeTreeProcedureBuilder builder{tree, tree_outputs};
  return builder.build();
}

}  // namespace blender::nodes
