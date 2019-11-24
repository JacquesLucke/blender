#include "builder.h"

namespace FN {

VTreeMFNetworkBuilder::VTreeMFNetworkBuilder(
    const VirtualNodeTree &vtree,
    const PreprocessedVTreeMFData &preprocessed_vtree_data,
    const VTreeMultiFunctionMappings &vtree_mappings,
    ResourceCollector &resources)
    : m_vtree(vtree),
      m_preprocessed_vtree_data(preprocessed_vtree_data),
      m_vtree_mappings(vtree_mappings),
      m_resources(resources),
      m_single_socket_by_vsocket(vtree.socket_count(), VTreeMFSocketMap_UNMAPPED),
      m_builder(BLI::make_unique<MFNetworkBuilder>())
{
}

MFBuilderFunctionNode &VTreeMFNetworkBuilder::add_function(const MultiFunction &function)
{
  return m_builder->add_function(function);
}

MFBuilderFunctionNode &VTreeMFNetworkBuilder::add_function(const MultiFunction &function,
                                                           const VNode &vnode)
{
  MFBuilderFunctionNode &node = m_builder->add_function(function);
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
  UNUSED_VARS_NDEBUG(vnode);
#ifdef DEBUG
  this->assert_data_sockets_are_mapped_correctly(vnode.inputs().cast<const VSocket *>());
  this->assert_data_sockets_are_mapped_correctly(vnode.outputs().cast<const VSocket *>());
#endif
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
  MFDataType vsocket_type = this->try_get_data_type(vsocket).value();
  UNUSED_VARS_NDEBUG(vsocket_type);

  if (vsocket.is_input()) {
    for (MFBuilderInputSocket *socket : this->lookup_socket(vsocket.as_input())) {
      MFDataType socket_type = socket->data_type();
      BLI_assert(socket_type == vsocket_type);
      UNUSED_VARS_NDEBUG(socket_type);
    }
  }
  else {
    MFBuilderSocket &socket = this->lookup_socket(vsocket.as_output());
    MFDataType socket_type = socket.data_type();
    BLI_assert(socket_type == vsocket_type);
    UNUSED_VARS_NDEBUG(socket_type);
  }
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

Vector<bool> VNodeMFNetworkBuilder::get_list_base_variadic_states(StringRefNull prop_name)
{
  Vector<bool> states;
  RNA_BEGIN (m_vnode.rna(), itemptr, prop_name.data()) {
    int state = RNA_enum_get(&itemptr, "state");
    if (state == 0) {
      /* single value case */
      states.append(false);
    }
    else if (state == 1) {
      /* list case */
      states.append(true);
    }
    else {
      BLI_assert(false);
    }
  }
  RNA_END;
  return states;
}

void VNodeMFNetworkBuilder::set_matching_fn(const MultiFunction &fn)
{
  MFBuilderFunctionNode &node = m_network_builder.add_function(fn);
  m_network_builder.map_data_sockets(m_vnode, node);
}

const MultiFunction &VNodeMFNetworkBuilder::get_vectorized_function(
    const MultiFunction &base_function, ArrayRef<const char *> is_vectorized_prop_names)
{
  Vector<bool> input_is_vectorized;
  for (const char *prop_name : is_vectorized_prop_names) {
    char state[5];
    RNA_string_get(m_vnode.rna(), prop_name, state);
    BLI_assert(STREQ(state, "BASE") || STREQ(state, "LIST"));

    bool is_vectorized = STREQ(state, "LIST");
    input_is_vectorized.append(is_vectorized);
  }

  if (input_is_vectorized.contains(true)) {
    return this->construct_fn<MF_SimpleVectorize>(base_function, input_is_vectorized);
  }
  else {
    return base_function;
  }
}

std::unique_ptr<VTreeMFNetwork> VTreeMFNetworkBuilder::build()
{
  // m_builder->to_dot__clipboard();

  auto network = BLI::make_unique<MFNetwork>(std::move(m_builder));

  Array<uint> vsocket_by_socket(network->socket_ids().size(), VTreeMFSocketMap_UNMAPPED);
  for (uint vsocket_id : m_single_socket_by_vsocket.index_iterator()) {
    switch (m_single_socket_by_vsocket[vsocket_id]) {
      case VTreeMFSocketMap_UNMAPPED: {
        break;
      }
      case VTreeMFSocketMap_MULTIMAPPED: {
        for (uint socket_id : m_multiple_inputs_by_vsocket.lookup(vsocket_id)) {
          vsocket_by_socket[socket_id] = vsocket_id;
        }
        break;
      }
      default: {
        uint socket_id = m_single_socket_by_vsocket[vsocket_id];
        vsocket_by_socket[socket_id] = vsocket_id;
        break;
      }
    }
  }

  VTreeMFSocketMap socket_map(m_vtree,
                              *network,
                              std::move(m_single_socket_by_vsocket),
                              std::move(m_multiple_inputs_by_vsocket),
                              std::move(vsocket_by_socket));

  return BLI::make_unique<VTreeMFNetwork>(m_vtree, std::move(network), std::move(socket_map));
}

}  // namespace FN
