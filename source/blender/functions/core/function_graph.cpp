#include "FN_core.hpp"

namespace FN {

Signature FunctionGraph::signature() const
{
  InputParameters inputs;
  OutputParameters outputs;

  for (const DFGraphSocket &socket : m_inputs) {
    inputs.append(
        InputParameter(m_graph->name_of_socket(socket), m_graph->type_of_socket(socket)));
  }
  for (const DFGraphSocket &socket : m_outputs) {
    outputs.append(
        OutputParameter(m_graph->name_of_socket(socket), m_graph->type_of_socket(socket)));
  }

  return Signature(inputs, outputs);
}

SmallSet<DFGraphSocket> FunctionGraph::find_used_sockets(bool include_inputs,
                                                         bool include_outputs) const
{
  SmallSet<DFGraphSocket> found;

  SmallSet<DFGraphSocket> to_be_checked;
  for (DFGraphSocket socket : m_outputs) {
    to_be_checked.add_new(socket);
  }

  while (to_be_checked.size() > 0) {
    DFGraphSocket socket = to_be_checked.pop();

    if (!include_inputs && m_inputs.contains(socket)) {
      continue;
    }

    found.add(socket);

    if (socket.is_input()) {
      to_be_checked.add_new(m_graph->origin_of_input(socket));
    }
    else {
      uint node_id = m_graph->node_id_of_output(socket.id());
      for (DFGraphSocket input_socket : this->graph()->inputs_of_node(node_id)) {
        to_be_checked.add_new(input_socket);
      }
    }
  }

  if (!include_outputs) {
    for (DFGraphSocket socket : m_outputs) {
      found.remove(socket);
    }
  }

  return found;
}

}  // namespace FN
