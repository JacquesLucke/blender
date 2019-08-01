#pragma once

#include <functional>

#include "BLI_optional.hpp"
#include "BLI_string_map.hpp"
#include "FN_tuple_call.hpp"

#include "vtree_data_graph_builder.hpp"

struct PointerRNA;

namespace FN {
namespace DataFlowNodes {

using StringPair = std::pair<std::string, std::string>;

typedef std::function<void(VTreeDataGraphBuilder &builder, VirtualNode *vnode)> NodeInserter;

typedef std::function<void(PointerRNA *socket_rna_ptr, Tuple &dst, uint index)> SocketLoader;

typedef std::function<void(
    VTreeDataGraphBuilder &builder, BuilderOutputSocket *from, BuilderInputSocket *to)>
    ConversionInserter;

typedef std::function<SharedFunction()> FunctionGetter;

StringMap<NodeInserter> &get_node_inserters_map();
StringMap<SocketLoader> &get_socket_loader_map();
Map<StringPair, ConversionInserter> &get_conversion_inserter_map();

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

 public:
  SocketLoaderRegistry(StringMap<SocketLoader> &map) : m_map(map)
  {
  }

  void loader(StringRef idname, SocketLoader loader)
  {
    m_map.add_new(idname, loader);
  }
};

class ConversionInserterRegistry {
 private:
  Map<StringPair, ConversionInserter> &m_map;

 public:
  ConversionInserterRegistry(Map<StringPair, ConversionInserter> &map) : m_map(map)
  {
  }

  void inserter(StringRef from_type, StringRef to_type, ConversionInserter inserter)
  {
    m_map.add_new(StringPair(from_type.to_std_string(), to_type.to_std_string()), inserter);
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
