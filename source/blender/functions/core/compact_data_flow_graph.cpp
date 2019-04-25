#include "FN_core.hpp"

namespace FN {

SharedCompactDataFlowGraph CompactDataFlowGraph::FromBuilder(
    DataFlowGraphBuilder &builder, CompactDataFlowGraph::ToBuilderMapping &r_mapping)
{
  auto graph = SharedCompactDataFlowGraph::New();
  auto dfgb_nodes = builder.nodes();

  graph->m_nodes.reserve(dfgb_nodes.size());

  const uint dummy = (uint)-1;

  for (DFGB_Node *dfgb_node : dfgb_nodes) {
    uint node_id = graph->m_nodes.size();
    graph->m_nodes.append(MyNode(dfgb_node->function(),
                                 dfgb_node->source(),
                                 graph->m_inputs.size(),
                                 graph->m_outputs.size()));
    r_mapping.node_indices.add_new(dfgb_node, node_id);

    for (DFGB_Socket dfgb_input : dfgb_node->inputs()) {
      r_mapping.input_socket_indices.add_new(dfgb_input, graph->m_inputs.size());
      graph->m_inputs.append(InputSocket(node_id, dummy));
    }
    for (DFGB_Socket output : dfgb_node->outputs()) {
      auto targets = output.targets();
      r_mapping.output_socket_indices.add_new(output, graph->m_outputs.size());
      graph->m_outputs.append(OutputSocket(node_id, graph->m_targets.size(), targets.size()));
      for (uint i = 0; i < targets.size(); i++) {
        graph->m_targets.append(dummy);
      }
    }
  }

  for (DFGB_Node *dfgb_node : dfgb_nodes) {
    for (DFGB_Socket dfgb_input : dfgb_node->inputs()) {
      uint input_id = r_mapping.input_socket_indices.lookup(dfgb_input);
      Optional<DFGB_Socket> dfgb_origin = dfgb_input.origin();
      BLI_assert(dfgb_origin.has_value());
      uint origin_id = r_mapping.output_socket_indices.lookup(dfgb_origin.value());
      graph->m_inputs[input_id].origin = origin_id;
    }
    for (DFGB_Socket dfgb_output : dfgb_node->outputs()) {
      uint output_id = r_mapping.output_socket_indices.lookup(dfgb_output);
      uint start = graph->m_outputs[output_id].targets_start;
      auto dfgb_targets = dfgb_output.targets();
      for (uint i = 0; i < dfgb_targets.size(); i++) {
        DFGB_Socket dfgb_target = dfgb_targets[i];
        uint target_id = r_mapping.input_socket_indices.lookup(dfgb_target);
        graph->m_targets[start + i] = target_id;
      }
    }
  }

  graph->m_source_info_pool = std::move(builder.m_source_info_pool);

  return graph;
}

CompactDataFlowGraph::~CompactDataFlowGraph()
{
  for (MyNode node : m_nodes) {
    if (node.source_info != nullptr) {
      node.source_info->~SourceInfo();
    }
  }
}

}  // namespace FN
