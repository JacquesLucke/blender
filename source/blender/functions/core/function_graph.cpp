#include "FN_core.hpp"

namespace FN {

SharedFunction FunctionGraph::new_function(StringRef name) const
{
  FunctionBuilder builder;
  builder.add_inputs(m_graph, m_inputs);
  builder.add_outputs(m_graph, m_outputs);
  return builder.build(name);
}

Set<DataSocket> FunctionGraph::find_used_sockets(bool include_inputs, bool include_outputs) const
{
  Set<DataSocket> found;

  VectorSet<DataSocket> to_be_checked;
  for (DataSocket socket : m_outputs) {
    to_be_checked.add_new(socket);
  }

  while (to_be_checked.size() > 0) {
    DataSocket socket = to_be_checked.pop();

    if (!include_inputs && m_inputs.contains(socket)) {
      continue;
    }

    found.add(socket);

    if (socket.is_input()) {
      to_be_checked.add_new(m_graph->origin_of_input(socket));
    }
    else {
      uint node_id = m_graph->node_id_of_output(socket.id());
      for (DataSocket input_socket : this->graph()->inputs_of_node(node_id)) {
        to_be_checked.add_new(input_socket);
      }
    }
  }

  if (!include_outputs) {
    for (DataSocket socket : m_outputs) {
      found.remove(socket);
    }
  }

  return found;
}

}  // namespace FN
