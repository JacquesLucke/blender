#include "inserters.hpp"
#include "registry.hpp"
#include "type_mappings.hpp"

#include "FN_dependencies.hpp"

#include "BLI_lazy_init.hpp"

#include "DNA_node_types.h"

#include "RNA_access.h"

namespace FN {
namespace DataFlowNodes {

using TypePair = std::pair<SharedType, SharedType>;

static void initialize_standard_inserters(GraphInserters &inserters)
{
  register_conversion_inserters(inserters);
}

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

BLI_LAZY_INIT(GraphInserters, get_standard_inserters)
{
  GraphInserters inserters;
  initialize_standard_inserters(inserters);
  return inserters;
}

GraphInserters::GraphInserters()
    : m_type_by_data_type(&get_type_by_data_type_map()),
      m_type_by_idname(&get_type_by_idname_map())
{
}

void GraphInserters::reg_conversion_inserter(StringRef from_type,
                                             StringRef to_type,
                                             ConversionInserter inserter)
{
  auto key = TypePair(m_type_by_data_type->lookup(from_type),
                      m_type_by_data_type->lookup(to_type));
  BLI_assert(!m_conversion_inserters.contains(key));
  m_conversion_inserters.add(key, inserter);
}

void GraphInserters::reg_conversion_function(StringRef from_type,
                                             StringRef to_type,
                                             FunctionGetter getter)
{
  auto inserter = [getter](VTreeDataGraphBuilder &builder, DFGB_Socket from, DFGB_Socket to) {
    auto fn = getter();
    DFGB_Node *node = builder.insert_function(fn);
    builder.insert_link(from, node->input(0));
    builder.insert_link(node->output(0), to);
  };
  this->reg_conversion_inserter(from_type, to_type, inserter);
}

bool GraphInserters::insert_link(VTreeDataGraphBuilder &builder,
                                 VirtualSocket *from_vsocket,
                                 VirtualSocket *to_vsocket)
{
  BLI_assert(builder.is_data_socket(from_vsocket));
  BLI_assert(builder.is_data_socket(to_vsocket));

  DFGB_Socket from_socket = builder.lookup_socket(from_vsocket);
  DFGB_Socket to_socket = builder.lookup_socket(to_vsocket);

  SharedType &from_type = builder.query_socket_type(from_vsocket);
  SharedType &to_type = builder.query_socket_type(to_vsocket);

  if (from_type == to_type) {
    builder.insert_link(from_socket, to_socket);
    return true;
  }

  auto key = TypePair(from_type, to_type);
  if (m_conversion_inserters.contains(key)) {
    auto inserter = m_conversion_inserters.lookup(key);
    inserter(builder, from_socket, to_socket);
    return true;
  }

  return false;
}

}  // namespace DataFlowNodes
}  // namespace FN

namespace std {
template<> struct hash<FN::DataFlowNodes::TypePair> {
  typedef FN::DataFlowNodes::TypePair argument_type;
  typedef size_t result_type;

  result_type operator()(argument_type const &v) const noexcept
  {
    size_t h1 = std::hash<FN::SharedType>{}(v.first);
    size_t h2 = std::hash<FN::SharedType>{}(v.second);
    return h1 ^ h2;
  }
};
}  // namespace std
