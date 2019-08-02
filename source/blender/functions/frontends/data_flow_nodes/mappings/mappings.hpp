#pragma once

#include <functional>

#include "BLI_string_map.hpp"
#include "FN_tuple_call.hpp"

#include "../vtree_data_graph_builder.hpp"

namespace FN {
namespace DataFlowNodes {

using BLI::StringMap;
using StringPair = std::pair<std::string, std::string>;

typedef std::function<void(VTreeDataGraphBuilder &builder, VirtualNode *vnode)> NodeInserter;

typedef std::function<void(PointerRNA *socket_rna_ptr, Tuple &dst, uint index)> SocketLoader;

typedef std::function<void(
    VTreeDataGraphBuilder &builder, BuilderOutputSocket *from, BuilderInputSocket *to)>
    ConversionInserter;

StringMap<SharedType> &MAPPING_type_by_idname();
StringMap<SharedType> &MAPPING_type_by_name();
StringMap<std::string> &MAPPING_type_name_by_idname();
StringMap<std::string> &MAPPING_type_idname_by_name();

StringMap<NodeInserter> &MAPPING_node_inserters();
StringMap<SocketLoader> &MAPPING_socket_loaders();
Map<StringPair, ConversionInserter> &MAPPING_conversion_inserters();

}  // namespace DataFlowNodes
}  // namespace FN

namespace std {
template<> struct hash<FN::DataFlowNodes::StringPair> {
  typedef FN::DataFlowNodes::StringPair argument_type;
  typedef size_t result_type;

  result_type operator()(argument_type const &v) const noexcept
  {
    size_t h1 = std::hash<std::string>{}(v.first);
    size_t h2 = std::hash<std::string>{}(v.second);
    return h1 ^ h2;
  }
};
}  // namespace std
