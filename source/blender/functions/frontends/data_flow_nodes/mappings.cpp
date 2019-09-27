#include "BLI_lazy_init_cxx.h"

#include "mappings.hpp"
#include "mappings/registry.hpp"

namespace FN {
namespace DataFlowNodes {

BLI_LAZY_INIT(std::unique_ptr<TypeMappings>, MAPPING_types)
{
  auto mappings = BLI::make_unique<TypeMappings>();
  REGISTER_type_mappings(mappings);
  return mappings;
}

BLI_LAZY_INIT(std::unique_ptr<NodeInserters>, MAPPING_node_inserters)
{
  auto inserters = BLI::make_unique<NodeInserters>();
  REGISTER_node_inserters(inserters);
  return inserters;
}

BLI_LAZY_INIT(std::unique_ptr<SocketLoaders>, MAPPING_socket_loaders)
{
  auto loaders = BLI::make_unique<SocketLoaders>();
  REGISTER_socket_loaders(loaders);
  return loaders;
}

BLI_LAZY_INIT(std::unique_ptr<LinkInserters>, MAPPING_link_inserters)
{
  auto inserters = BLI::make_unique<LinkInserters>();
  REGISTER_conversion_inserters(inserters);
  return inserters;
}

void TypeMappings::register_type(StringRef idname, StringRef name, Type *type)
{
  m_type_by_idname.add_new(idname, type);
  m_type_by_name.add_new(name, type);
  m_name_by_idname.add_new(idname, name);
  m_idname_by_name.add_new(name, idname);
}

void NodeInserters::register_inserter(StringRef idname, NodeInserter inserter)
{
  m_inserter_by_idname.add_new(idname, inserter);
}

void NodeInserters::register_function(StringRef idname, FunctionGetter getter)
{
  auto inserter = [getter](VTreeDataGraphBuilder &builder, VirtualNode *vnode) {
    Function &fn = getter();
    BuilderNode *node = builder.insert_function(fn, vnode);
    builder.map_sockets(node, vnode);
  };
  this->register_inserter(idname, inserter);
}

bool NodeInserters::insert(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  NodeInserter *inserter = m_inserter_by_idname.lookup_ptr(vnode->idname());
  if (inserter == nullptr) {
    return false;
  }
  (*inserter)(builder, vnode);
  return true;
}

SocketLoaders::SocketLoaders() : m_type_mappings(MAPPING_types())
{
}

void SocketLoaders::register_loader(StringRef type_name, SocketLoader loader)
{
  StringRef idname = m_type_mappings->idname_by_name(type_name);
  m_loader_by_idname.add_new(idname, loader);
}

void SocketLoaders::load(VirtualSocket *vsocket, Tuple &dst, uint index)
{
  SocketLoader &loader = m_loader_by_idname.lookup(vsocket->idname());
  PointerRNA rna = vsocket->rna();
  loader(&rna, dst, index);
}

LinkInserters::LinkInserters() : m_type_mappings(MAPPING_types())
{
}

void LinkInserters::register_conversion_inserter(StringRef from_type,
                                                 StringRef to_type,
                                                 ConversionInserter inserter)
{
  StringRef from_idname = m_type_mappings->idname_by_name(from_type);
  StringRef to_idname = m_type_mappings->idname_by_name(to_type);
  m_conversion_inserters.add_new(StringPair(from_idname, to_idname), inserter);
}

void LinkInserters::register_conversion_function(StringRef from_type,
                                                 StringRef to_type,
                                                 FunctionGetter getter)
{
  auto inserter =
      [getter](VTreeDataGraphBuilder &builder, BuilderOutputSocket *from, BuilderInputSocket *to) {
        Function &fn = getter();
        BuilderNode *node = builder.insert_function(fn);
        builder.insert_link(from, node->input(0));
        builder.insert_link(node->output(0), to);
      };
  this->register_conversion_inserter(from_type, to_type, inserter);
}

bool LinkInserters::insert(VTreeDataGraphBuilder &builder, VirtualSocket *from, VirtualSocket *to)
{
  BLI_assert(from->is_output());
  BLI_assert(to->is_input());
  BLI_assert(builder.is_data_socket(from));
  BLI_assert(builder.is_data_socket(to));

  BuilderOutputSocket *from_socket = builder.lookup_output_socket(from);
  BuilderInputSocket *to_socket = builder.lookup_input_socket(to);

  if (from->idname() == to->idname()) {
    builder.insert_link(from_socket, to_socket);
    return true;
  }

  StringPair key(from->idname(), to->idname());
  ConversionInserter *inserter = m_conversion_inserters.lookup_ptr(key);
  if (inserter != nullptr) {
    (*inserter)(builder, from_socket, to_socket);
    return true;
  }

  return false;
}

}  // namespace DataFlowNodes
}  // namespace FN
