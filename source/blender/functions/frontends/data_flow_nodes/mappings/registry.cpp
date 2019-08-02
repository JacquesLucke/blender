#include "BLI_lazy_init.hpp"

#include "registry.hpp"

namespace FN {
namespace DataFlowNodes {

BLI_LAZY_INIT(StringMap<NodeInserter>, get_node_inserters_map)
{
  StringMap<NodeInserter> map;
  NodeInserterRegistry registry(map);
  register_node_inserters(registry);
  return map;
}

BLI_LAZY_INIT(StringMap<SocketLoader>, get_socket_loader_map)
{
  StringMap<SocketLoader> map;
  SocketLoaderRegistry registry(map);
  register_socket_loaders(registry);
  return map;
}

using ConversionInserterMap = Map<StringPair, ConversionInserter>;
BLI_LAZY_INIT(ConversionInserterMap, get_conversion_inserter_map)
{
  Map<StringPair, ConversionInserter> map;
  ConversionInserterRegistry registry(map);
  register_conversion_inserters(registry);
  return map;
}

}  // namespace DataFlowNodes
}  // namespace FN
