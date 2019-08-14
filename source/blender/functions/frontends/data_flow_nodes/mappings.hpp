#pragma once

#include <functional>

#include "BLI_string_map.hpp"
#include "FN_tuple_call.hpp"
#include "BKE_node_tree.hpp"

namespace FN {
namespace DataFlowNodes {

using BKE::VirtualNode;
using BKE::VirtualSocket;
using StringPair = std::pair<std::string, std::string>;

class VTreeDataGraphBuilder;

typedef std::function<void(VTreeDataGraphBuilder &builder, VirtualNode *vnode)> NodeInserter;

typedef std::function<void(PointerRNA *socket_rna_ptr, Tuple &dst, uint index)> SocketLoader;

typedef std::function<void(
    VTreeDataGraphBuilder &builder, BuilderOutputSocket *from, BuilderInputSocket *to)>
    ConversionInserter;

typedef std::function<SharedFunction()> FunctionGetter;

class TypeMappings {
 private:
  StringMap<Type *> m_type_by_idname;
  StringMap<Type *> m_type_by_name;
  StringMap<std::string> m_name_by_idname;
  StringMap<std::string> m_idname_by_name;

 public:
  void register_type(StringRef idname, StringRef name, Type *type);

  Type *type_by_idname_or_empty(StringRef idname)
  {
    return m_type_by_idname.lookup_default(idname, {});
  }

  Type *type_by_idname(StringRef idname)
  {
    return m_type_by_idname.lookup(idname);
  }

  Type *type_by_name(StringRef name)
  {
    return m_type_by_name.lookup(name);
  }

  StringRefNull name_by_idname(StringRef idname)
  {
    return m_name_by_idname.lookup(idname);
  }

  StringRefNull idname_by_name(StringRef name)
  {
    return m_idname_by_name.lookup(name);
  }
};

class NodeInserters {
 private:
  StringMap<NodeInserter> m_inserter_by_idname;

 public:
  void register_inserter(StringRef idname, NodeInserter inserter);
  void register_function(StringRef idname, FunctionGetter getter);

  bool insert(VTreeDataGraphBuilder &builder, VirtualNode *vnode);
};

class LinkInserters {
 private:
  std::unique_ptr<TypeMappings> &m_type_mappings;
  Map<StringPair, ConversionInserter> m_conversion_inserters;

 public:
  LinkInserters();

  void register_conversion_inserter(StringRef from_type,
                                    StringRef to_type,
                                    ConversionInserter inserter);
  void register_conversion_function(StringRef from_type, StringRef to_type, FunctionGetter getter);

  bool insert(VTreeDataGraphBuilder &builder, VirtualSocket *from, VirtualSocket *to);
};

class SocketLoaders {
 private:
  std::unique_ptr<TypeMappings> &m_type_mappings;
  StringMap<SocketLoader> m_loader_by_idname;

 public:
  SocketLoaders();

  void register_loader(StringRef type_name, SocketLoader loader);

  void load(VirtualSocket *vsocket, Tuple &dst, uint index);
  SocketLoader get_loader(StringRef idname)
  {
    return m_loader_by_idname.lookup(idname);
  }
};

std::unique_ptr<TypeMappings> &MAPPING_types(void);
std::unique_ptr<NodeInserters> &MAPPING_node_inserters(void);
std::unique_ptr<SocketLoaders> &MAPPING_socket_loaders(void);
std::unique_ptr<LinkInserters> &MAPPING_link_inserters(void);

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
