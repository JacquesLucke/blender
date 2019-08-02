#pragma once

#include "mappings.hpp"

struct PointerRNA;

namespace FN {
namespace DataFlowNodes {

typedef std::function<SharedFunction()> FunctionGetter;

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
      BuilderNode *node = builder.insert_function(fn, vnode);
      builder.map_sockets(node, vnode);
    };
    this->inserter(idname, inserter);
  }
};

class SocketLoaderRegistry {
 private:
  StringMap<SocketLoader> &m_map;
  StringMap<std::string> &m_idname_by_data_type;

 public:
  SocketLoaderRegistry(StringMap<SocketLoader> &map)
      : m_map(map), m_idname_by_data_type(get_idname_by_data_type_map())
  {
  }

  void loader(StringRef data_type, SocketLoader loader)
  {
    std::string &idname = m_idname_by_data_type.lookup_ref(data_type);
    m_map.add_new(idname, loader);
  }
};

class ConversionInserterRegistry {
 private:
  Map<StringPair, ConversionInserter> &m_map;
  StringMap<std::string> &m_idname_by_data_type;

 public:
  ConversionInserterRegistry(Map<StringPair, ConversionInserter> &map)
      : m_map(map), m_idname_by_data_type(get_idname_by_data_type_map())
  {
  }

  void inserter(StringRef from_type, StringRef to_type, ConversionInserter inserter)
  {
    std::string &from_idname = m_idname_by_data_type.lookup_ref(from_type);
    std::string &to_idname = m_idname_by_data_type.lookup_ref(to_type);
    m_map.add_new(StringPair(from_idname, to_idname), inserter);
  }
  void function(StringRef from_type, StringRef to_type, FunctionGetter getter)
  {
    auto inserter = [getter](VTreeDataGraphBuilder &builder,
                             BuilderOutputSocket *from,
                             BuilderInputSocket *to) {
      auto fn = getter();
      BuilderNode *node = builder.insert_function(fn);
      builder.insert_link(from, node->input(0));
      builder.insert_link(node->output(0), to);
    };
    this->inserter(from_type, to_type, inserter);
  }
};

void register_socket_loaders(SocketLoaderRegistry &registry);
void register_node_inserters(NodeInserterRegistry &registry);
void register_conversion_inserters(ConversionInserterRegistry &registry);

}  // namespace DataFlowNodes
}  // namespace FN
