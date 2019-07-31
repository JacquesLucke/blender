#pragma once

#include "inserters.hpp"

namespace FN {
namespace DataFlowNodes {

void initialize_socket_inserters(GraphInserters &inserters);
void register_node_inserters(NodeInserterRegistry &registry);
void register_conversion_inserters(GraphInserters &inserters);

}  // namespace DataFlowNodes
}  // namespace FN
