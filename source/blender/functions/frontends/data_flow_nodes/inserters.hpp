#pragma once

#include <functional>

#include "BLI_optional.hpp"
#include "BLI_string_map.hpp"
#include "FN_tuple_call.hpp"

#include "vtree_data_graph_builder.hpp"

struct PointerRNA;

namespace FN {
namespace DataFlowNodes {

typedef std::function<void(VTreeDataGraphBuilder &builder, VirtualNode *vnode)> NodeInserter;

typedef std::function<void(PointerRNA *socket_rna_ptr, Tuple &dst, uint index)> SocketLoader;

typedef std::function<void(VTreeDataGraphBuilder &builder, DFGB_Socket from, DFGB_Socket to)>
    ConversionInserter;

typedef std::function<SharedFunction()> FunctionGetter;

StringMap<NodeInserter> &get_node_inserters_map();

class NodeInserterRegistry {
 private:
  StringMap<NodeInserter> &m_map;

 public:
  NodeInserterRegistry(StringMap<NodeInserter> &map) : m_map(map)
  {
  }

  void inserter(StringRef idname, NodeInserter inserter)
  {
    m_map.add_new(idname, inserter);
  }
  void function(StringRef idname, FunctionGetter getter)
  {
    auto inserter = [getter](VTreeDataGraphBuilder &builder, VirtualNode *vnode) {
      SharedFunction fn = getter();
      DFGB_Node *node = builder.insert_function(fn, vnode);
      builder.map_sockets(node, vnode);
    };
    this->inserter(idname, inserter);
  }
};

class GraphInserters {
 private:
  StringMap<SocketLoader> m_socket_loaders;
  Map<std::pair<SharedType, SharedType>, ConversionInserter> m_conversion_inserters;
  StringMap<SharedType> *m_type_by_data_type;
  StringMap<SharedType> *m_type_by_idname;

 public:
  GraphInserters();

  void reg_socket_loader(std::string idname, SocketLoader loader);

  void reg_conversion_inserter(StringRef from_type,
                               StringRef to_type,
                               ConversionInserter inserter);

  void reg_conversion_function(StringRef from_type, StringRef to_type, FunctionGetter getter);

  void insert_sockets(VTreeDataGraphBuilder &builder,
                      ArrayRef<VirtualSocket *> vsockets,
                      ArrayRef<DFGB_Socket> r_new_origins);

  bool insert_link(VTreeDataGraphBuilder &builder,
                   VirtualSocket *from_vsocket,
                   VirtualSocket *to_vsocket);
};

GraphInserters &get_standard_inserters();

}  // namespace DataFlowNodes
}  // namespace FN
