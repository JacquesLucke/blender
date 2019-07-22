#include "FN_core.hpp"

namespace FN {

SharedFunction FunctionGraph::new_function(StringRef name) const
{
  FunctionBuilder builder;

  for (const DFGraphSocket &socket : m_inputs) {
    builder.add_input(m_graph->name_of_socket(socket), m_graph->type_of_socket(socket));
  }
  for (const DFGraphSocket &socket : m_outputs) {
    builder.add_output(m_graph->name_of_socket(socket), m_graph->type_of_socket(socket));
  }

  return builder.build(name);
}

Set<DFGraphSocket> FunctionGraph::find_used_sockets(bool include_inputs,
                                                    bool include_outputs) const
{
  Set<DFGraphSocket> found;

  Set<DFGraphSocket> to_be_checked;
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
