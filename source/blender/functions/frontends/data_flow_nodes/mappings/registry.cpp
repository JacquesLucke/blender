#include "BLI_lazy_init.hpp"

#include "registry.hpp"

namespace FN {
namespace DataFlowNodes {

BLI_LAZY_INIT(StringMap<NodeInserter>, MAPPING_node_inserters)
{
  StringMap<NodeInserter> map;
  NodeInserterRegistry registry(map);
  REGISTER_node_inserters(registry);
  return map;
}

BLI_LAZY_INIT(StringMap<SocketLoader>, MAPPING_socket_loaders)
{
  StringMap<SocketLoader> map;
  SocketLoaderRegistry registry(map);
  REGISTER_socket_loaders(registry);
  return map;
}

using ConversionInserterMap = Map<StringPair, ConversionInserter>;
BLI_LAZY_INIT(ConversionInserterMap, MAPPING_conversion_inserters)
{
  Map<StringPair, ConversionInserter> map;
  ConversionInserterRegistry registry(map);
  REGISTER_conversion_inserters(registry);
  return map;
}

}  // namespace DataFlowNodes
}  // namespace FN
