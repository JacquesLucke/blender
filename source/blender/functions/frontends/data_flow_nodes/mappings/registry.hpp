#pragma once

#include "../mappings.hpp"
#include "../vtree_data_graph_builder.hpp"

struct PointerRNA;

namespace FN {
namespace DataFlowNodes {

void REGISTER_type_mappings(std::unique_ptr<TypeMappings> &type_mappings);
void REGISTER_socket_loaders(std::unique_ptr<SocketLoaders> &loaders);
void REGISTER_node_inserters(std::unique_ptr<NodeInserters> &inserters);
void REGISTER_conversion_inserters(std::unique_ptr<LinkInserters> &inserters);

}  // namespace DataFlowNodes
}  // namespace FN
