#pragma once

#include "inserters.hpp"

namespace FN {
namespace DataFlowNodes {

void register_socket_loaders(SocketLoaderRegistry &registry);
void register_node_inserters(NodeInserterRegistry &registry);
void register_conversion_inserters(ConversionInserterRegistry &registry);

}  // namespace DataFlowNodes
}  // namespace FN
