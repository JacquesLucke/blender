#include "FN_core.hpp"

namespace FN {

DataGraph::DataGraph(std::unique_ptr<OwnedResources> resources,
                     Vector<Node> nodes,
                     Vector<InputSocket> inputs,
                     Vector<OutputSocket> outputs,
                     Vector<uint> targets,
                     std::unique_ptr<MonotonicAllocator> source_info_allocator)
    : m_resources(std::move(resources)),
      m_nodes(std::move(nodes)),
      m_inputs(std::move(inputs)),
      m_outputs(std::move(outputs)),
      m_targets(std::move(targets)),
      m_source_info_allocator(std::move(source_info_allocator))
{
}

DataGraph::~DataGraph()
{
  for (Node node : m_nodes) {
    if (node.source_info != nullptr) {
      node.source_info->~SourceInfo();
    }
  }
}

void DataGraph::print_socket(DataSocket socket) const
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

std::string DataGraph::to_dot()
{
  DataGraphBuilder builder;
  this->insert_in_builder(builder);
  return builder.to_dot();
}

void DataGraph::to_dot__clipboard()
{
  DataGraphBuilder builder;
  this->insert_in_builder(builder);
  builder.to_dot__clipboard();
}

void DataGraph::insert_in_builder(DataGraphBuilder &builder)
{
  Vector<BuilderNode *> builder_nodes;

  for (auto &node : m_nodes) {
    BuilderNode *builder_node = builder.insert_function(node.function);
    builder_nodes.append(builder_node);
  }

  for (uint input_id = 0; input_id < m_inputs.size(); input_id++) {
    uint from_id = m_inputs[input_id].origin;
    uint from_node_id = m_outputs[from_id].node;
    uint from_index = this->index_of_output(from_id);
    BuilderNode *from_builder_node = builder_nodes[from_node_id];
    auto *from_builder_socket = from_builder_node->outputs()[from_index];

    uint to_id = input_id;
    uint to_node_id = m_inputs[to_id].node;
    uint to_index = this->index_of_input(to_id);
    BuilderNode *to_builder_node = builder_nodes[to_node_id];
    auto *to_builder_socket = to_builder_node->inputs()[to_index];

    builder.insert_link(from_builder_socket, to_builder_socket);
  }
}

}  // namespace FN
