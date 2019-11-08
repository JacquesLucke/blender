#include "builder.h"

namespace FN {

VTreeMFNetworkBuilder::VTreeMFNetworkBuilder(const VirtualNodeTree &vtree,
                                             const VTreeMultiFunctionMappings &vtree_mappings,
                                             ResourceCollector &resources)
    : m_vtree(vtree),
      m_vtree_mappings(vtree_mappings),
      m_resources(resources),
      m_socket_map(vtree.socket_count(), nullptr),
      m_type_by_vsocket(vtree.socket_count()),
      m_builder(BLI::make_unique<MFNetworkBuilder>())
{
  for (const VSocket *vsocket : vtree.all_sockets()) {
    const MFDataType *data_type = vtree_mappings.data_type_by_idname.lookup_ptr(vsocket->idname());
    if (data_type == nullptr) {
      m_type_by_vsocket[vsocket->id()] = {};
    }
    else {
      m_type_by_vsocket[vsocket->id()] = MFDataType(*data_type);
    }
  }
}

MFBuilderFunctionNode &VTreeMFNetworkBuilder::add_function(const MultiFunction &function,
                                                           ArrayRef<uint> input_param_indices,
                                                           ArrayRef<uint> output_param_indices)
{
  return m_builder->add_function(function, input_param_indices, output_param_indices);
}

MFBuilderFunctionNode &VTreeMFNetworkBuilder::add_function(const MultiFunction &function,
                                                           ArrayRef<uint> input_param_indices,
                                                           ArrayRef<uint> output_param_indices,
                                                           const VNode &vnode)
{
  MFBuilderFunctionNode &node = m_builder->add_function(
      function, input_param_indices, output_param_indices);
  this->map_data_sockets(vnode, node);
  return node;
}

MFBuilderDummyNode &VTreeMFNetworkBuilder::add_dummy(const VNode &vnode)
{
  Vector<MFDataType> input_types;
  for (const VInputSocket *vsocket : vnode.inputs()) {
    Optional<MFDataType> data_type = this->try_get_data_type(*vsocket);
    if (data_type.has_value()) {
      input_types.append(data_type.value());
    }
  }

  Vector<MFDataType> output_types;
  for (const VOutputSocket *vsocket : vnode.outputs()) {
    Optional<MFDataType> data_type = this->try_get_data_type(*vsocket);
    if (data_type.has_value()) {
      output_types.append(data_type.value());
    }
  }

  MFBuilderDummyNode &node = m_builder->add_dummy(input_types, output_types);
  this->map_data_sockets(vnode, node);
  return node;
}

void VTreeMFNetworkBuilder::map_data_sockets(const VNode &vnode, MFBuilderNode &node)
{
  uint data_inputs = 0;
  for (const VInputSocket *vsocket : vnode.inputs()) {
    if (this->is_data_socket(*vsocket)) {
      this->map_sockets(*vsocket, *node.inputs()[data_inputs]);
      data_inputs++;
    }
  }

  uint data_outputs = 0;
  for (const VOutputSocket *vsocket : vnode.outputs()) {
    if (this->is_data_socket(*vsocket)) {
      this->map_sockets(*vsocket, *node.outputs()[data_outputs]);
      data_outputs++;
    }
  }
}

void VTreeMFNetworkBuilder::assert_vnode_is_mapped_correctly(const VNode &vnode) const
{
  this->assert_data_sockets_are_mapped_correctly(vnode.inputs().cast<const VSocket *>());
  this->assert_data_sockets_are_mapped_correctly(vnode.outputs().cast<const VSocket *>());
}

void VTreeMFNetworkBuilder::assert_data_sockets_are_mapped_correctly(
    ArrayRef<const VSocket *> vsockets) const
{
  for (const VSocket *vsocket : vsockets) {
    if (this->is_data_socket(*vsocket)) {
      this->assert_vsocket_is_mapped_correctly(*vsocket);
    }
  }
}

void VTreeMFNetworkBuilder::assert_vsocket_is_mapped_correctly(const VSocket &vsocket) const
{
  BLI_assert(this->vsocket_is_mapped(vsocket));
  MFBuilderSocket &socket = this->lookup_socket(vsocket);
  MFDataType socket_type = socket.type();
  MFDataType vsocket_type = this->try_get_data_type(vsocket).value();
  BLI_assert(socket_type == vsocket_type);
  UNUSED_VARS_NDEBUG(socket_type, vsocket_type);
}

bool VTreeMFNetworkBuilder::has_data_sockets(const VNode &vnode) const
{
  for (const VInputSocket *vsocket : vnode.inputs()) {
    if (this->is_data_socket(*vsocket)) {
      return true;
    }
  }
  for (const VOutputSocket *vsocket : vnode.outputs()) {
    if (this->is_data_socket(*vsocket)) {
      return true;
    }
  }
  return false;
}

const CPPType &VTreeMFNetworkBuilder::cpp_type_from_property(const VNode &vnode,
                                                             StringRefNull prop_name) const
{
  char *type_name = RNA_string_get_alloc(vnode.rna(), prop_name.data(), nullptr, 0);
  const CPPType &type = this->cpp_type_by_name(type_name);
  MEM_freeN(type_name);
  return type;
}

MFDataType VTreeMFNetworkBuilder::data_type_from_property(const VNode &vnode,
                                                          StringRefNull prop_name) const
{
  char *type_name = RNA_string_get_alloc(vnode.rna(), prop_name.data(), nullptr, 0);
  MFDataType type = m_vtree_mappings.data_type_by_type_name.lookup(type_name);
  MEM_freeN(type_name);
  return type;
}

std::unique_ptr<VTreeMFNetwork> VTreeMFNetworkBuilder::build()
{
  // m_builder->to_dot__clipboard();

  Array<int> socket_ids(m_vtree.socket_count(), -1);
  for (uint vsocket_id = 0; vsocket_id < m_vtree.socket_count(); vsocket_id++) {
    MFBuilderSocket *builder_socket = m_socket_map[vsocket_id];
    if (builder_socket != nullptr) {
      socket_ids[vsocket_id] = builder_socket->id();
    }
  }

  auto network = BLI::make_unique<MFNetwork>(std::move(m_builder));

  Array<const MFSocket *> socket_map(m_vtree.socket_count(), nullptr);
  for (uint vsocket_id = 0; vsocket_id < m_vtree.socket_count(); vsocket_id++) {
    int id = socket_ids[vsocket_id];
    if (id != -1) {
      socket_map[vsocket_id] = &network->socket_by_id(socket_ids[vsocket_id]);
    }
  }

  return BLI::make_unique<VTreeMFNetwork>(m_vtree, std::move(network), std::move(socket_map));
}

}  // namespace FN
