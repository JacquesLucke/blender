#include "FN_core.hpp"

namespace FN {

DataFlowGraph::BuildResult DataFlowGraph::FromBuilder(DataFlowGraphBuilder &builder)
{
  BuildResult result = {SharedDataFlowGraph::New(), {}};

  SharedDataFlowGraph &graph = result.graph;
  ToBuilderMapping &mapping = result.mapping;

  auto dfgb_nodes = builder.nodes();

  graph->m_nodes.reserve(dfgb_nodes.size());

  const uint dummy = (uint)-1;

  for (DFGB_Node *dfgb_node : dfgb_nodes) {
    uint node_id = graph->m_nodes.size();
    graph->m_nodes.append(MyNode(dfgb_node->function(),
                                 dfgb_node->source(),
                                 graph->m_inputs.size(),
                                 graph->m_outputs.size()));
    mapping.node_indices.add_new(dfgb_node, node_id);

    for (DFGB_Socket dfgb_input : dfgb_node->inputs()) {
      mapping.input_socket_indices.add_new(dfgb_input, graph->m_inputs.size());
      graph->m_inputs.append(InputSocket(node_id, dummy));
    }
    for (DFGB_Socket output : dfgb_node->outputs()) {
      auto targets = output.targets();
      mapping.output_socket_indices.add_new(output, graph->m_outputs.size());
      graph->m_outputs.append(OutputSocket(node_id, graph->m_targets.size(), targets.size()));
      for (uint i = 0; i < targets.size(); i++) {
        graph->m_targets.append(dummy);
      }
    }
  }

  for (DFGB_Node *dfgb_node : dfgb_nodes) {
    for (DFGB_Socket dfgb_input : dfgb_node->inputs()) {
      uint input_id = mapping.input_socket_indices.lookup(dfgb_input);
      Optional<DFGB_Socket> dfgb_origin = dfgb_input.origin();
      BLI_assert(dfgb_origin.has_value());
      uint origin_id = mapping.output_socket_indices.lookup(dfgb_origin.value());
      graph->m_inputs[input_id].origin = origin_id;
    }
    for (DFGB_Socket dfgb_output : dfgb_node->outputs()) {
      uint output_id = mapping.output_socket_indices.lookup(dfgb_output);
      uint start = graph->m_outputs[output_id].targets_start;
      auto dfgb_targets = dfgb_output.targets();
      for (uint i = 0; i < dfgb_targets.size(); i++) {
        DFGB_Socket dfgb_target = dfgb_targets[i];
        uint target_id = mapping.input_socket_indices.lookup(dfgb_target);
        graph->m_targets[start + i] = target_id;
      }
    }
  }

  graph->m_source_info_pool = std::move(builder.m_source_info_pool);

  return result;
}

DataFlowGraph::~DataFlowGraph()
{
  for (MyNode node : m_nodes) {
    if (node.source_info != nullptr) {
      node.source_info->~SourceInfo();
    }
  }
}

void DataFlowGraph::print_socket(DFGraphSocket socket) const
{
  uint node_id = this->node_id_of_socket(socket);
  auto &node = m_nodes[node_id];
  std::cout << "<" << node.function->name() << " - ";
  if (socket.is_input()) {
    std::cout << "Input";
  }
  else {
    std::cout << "Output";
  }
  std::cout << ":" << this->index_of_socket(socket) << ">";
}

std::string DataFlowGraph::to_dot()
{
  DataFlowGraphBuilder builder;
  this->insert_in_builder(builder);
  return builder.to_dot();
}

void DataFlowGraph::to_dot__clipboard()
{
  DataFlowGraphBuilder builder;
  this->insert_in_builder(builder);
  builder.to_dot__clipboard();
}

void DataFlowGraph::insert_in_builder(DataFlowGraphBuilder &builder)
{
  SmallVector<DFGB_Node *> dfgb_nodes;

  for (auto &node : m_nodes) {
    DFGB_Node *dfgb_node = builder.insert_function(node.function);
    dfgb_nodes.append(dfgb_node);
  }

  for (uint input_id = 0; input_id < m_inputs.size(); input_id++) {
    uint from_id = m_inputs[input_id].origin;
    uint from_node_id = m_outputs[from_id].node;
    uint from_index = this->index_of_output(from_id);
    DFGB_Node *from_dfgb_node = dfgb_nodes[from_node_id];
    DFGB_Socket from_dfgb_socket = from_dfgb_node->output(from_index);

    uint to_id = input_id;
    uint to_node_id = m_inputs[to_id].node;
    uint to_index = this->index_of_input(to_id);
    DFGB_Node *to_dfgb_node = dfgb_nodes[to_node_id];
    DFGB_Socket to_dfgb_socket = to_dfgb_node->input(to_index);

    builder.insert_link(from_dfgb_socket, to_dfgb_socket);
  }
}

}  // namespace FN
