#include "FN_core.hpp"

namespace FN {

Signature CompactFunctionGraph::signature() const
{
  InputParameters inputs;
  OutputParameters outputs;

  for (const FunctionSocket &socket : m_inputs) {
    inputs.append(
        InputParameter(m_graph->name_of_socket(socket), m_graph->type_of_socket(socket)));
  }
  for (const FunctionSocket &socket : m_outputs) {
    outputs.append(
        OutputParameter(m_graph->name_of_socket(socket), m_graph->type_of_socket(socket)));
  }

  return Signature(inputs, outputs);
}

SmallSet<FunctionSocket> CompactFunctionGraph::find_used_sockets(bool include_inputs,
                                                                 bool include_outputs) const
{
  SmallSet<FunctionSocket> found;

  SmallSet<FunctionSocket> to_be_checked;
  for (FunctionSocket socket : m_outputs) {
    to_be_checked.add_new(socket);
  }

  while (to_be_checked.size() > 0) {
    FunctionSocket socket = to_be_checked.pop();

    if (!include_inputs && m_inputs.contains(socket)) {
      continue;
    }

    found.add(socket);

    if (socket.is_input()) {
      to_be_checked.add_new(m_graph->origin_of_input(socket));
    }
    else {
      uint node_id = m_graph->node_id_of_output(socket.id());
      for (FunctionSocket input_socket : this->graph()->inputs_of_node(node_id)) {
        to_be_checked.add_new(input_socket);
      }
    }
  }

  if (!include_outputs) {
    for (FunctionSocket socket : m_outputs) {
      found.remove(socket);
    }
  }

  return found;
}

}  // namespace FN
