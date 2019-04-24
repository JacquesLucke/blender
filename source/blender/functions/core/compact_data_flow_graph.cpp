#include "compact_data_flow_graph.hpp"

namespace FN {

CompactDataFlowGraph::CompactDataFlowGraph(std::unique_ptr<DataFlowGraph> orig_graph)
    : m_builder(std::move(orig_graph))
{
  m_nodes.reserve(m_builder->all_nodes().size());

  const uint dummy = (uint)-1;

  for (Node *node : m_builder->all_nodes()) {
    uint node_index = m_nodes.size();
    m_nodes.append(MyNode(node->function(), node->source(), m_inputs.size(), m_outputs.size()));
    m_node_indices.add_new(node, node_index);

    for (Socket input : node->inputs()) {
      m_input_socket_indices.add_new(input, m_inputs.size());
      m_inputs.append(InputSocket(node_index, dummy));
    }
    for (Socket output : node->outputs()) {
      auto targets = output.targets();
      m_output_socket_indices.add_new(output, m_outputs.size());
      m_outputs.append(OutputSocket(node_index, m_targets.size(), targets.size()));
      for (uint i = 0; i < targets.size(); i++) {
        m_targets.append(dummy);
      }
    }
  }

  for (Node *node : m_builder->all_nodes()) {
    for (Socket input : node->inputs()) {
      uint input_index = m_input_socket_indices.lookup(input);
      uint origin_index = m_output_socket_indices.lookup(input.origin());
      m_inputs[input_index].origin = origin_index;
    }
    for (Socket output : node->outputs()) {
      uint output_index = m_output_socket_indices.lookup(output);
      uint start = m_outputs[output_index].targets_start;
      auto targets = output.targets();
      for (uint i = 0; i < targets.size(); i++) {
        Socket target = targets[i];
        uint target_index = m_input_socket_indices.lookup(target);
        m_targets[start + i] = target_index;
      }
    }
  }
}

}  // namespace FN
