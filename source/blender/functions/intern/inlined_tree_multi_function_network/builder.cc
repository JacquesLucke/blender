#include "builder.h"

namespace FN {

VTreeMFNetworkBuilder::VTreeMFNetworkBuilder(
    const InlinedNodeTree &inlined_tree,
    const PreprocessedVTreeMFData &preprocessed_inlined_tree_data,
    const VTreeMultiFunctionMappings &inlined_tree_mappings,
    ResourceCollector &resources)
    : m_inlined_tree(inlined_tree),
      m_preprocessed_inlined_tree_data(preprocessed_inlined_tree_data),
      m_inlined_tree_mappings(inlined_tree_mappings),
      m_resources(resources),
      m_single_socket_by_xsocket(inlined_tree.socket_count(), VTreeMFSocketMap_UNMAPPED),
      m_builder(BLI::make_unique<MFNetworkBuilder>())
{
}

MFBuilderFunctionNode &VTreeMFNetworkBuilder::add_function(const MultiFunction &function)
{
  return m_builder->add_function(function);
}

MFBuilderFunctionNode &VTreeMFNetworkBuilder::add_function(const MultiFunction &function,
                                                           const XNode &xnode)
{
  MFBuilderFunctionNode &node = m_builder->add_function(function);
  this->map_data_sockets(xnode, node);
  return node;
}

MFBuilderDummyNode &VTreeMFNetworkBuilder::add_dummy(const XNode &xnode)
{
  Vector<MFDataType> input_types;
  for (const XInputSocket *xsocket : xnode.inputs()) {
    Optional<MFDataType> data_type = this->try_get_data_type(*xsocket);
    if (data_type.has_value()) {
      input_types.append(data_type.value());
    }
  }

  Vector<MFDataType> output_types;
  for (const XOutputSocket *xsocket : xnode.outputs()) {
    Optional<MFDataType> data_type = this->try_get_data_type(*xsocket);
    if (data_type.has_value()) {
      output_types.append(data_type.value());
    }
  }

  MFBuilderDummyNode &node = m_builder->add_dummy(input_types, output_types);
  this->map_data_sockets(xnode, node);
  return node;
}

void VTreeMFNetworkBuilder::map_data_sockets(const XNode &xnode, MFBuilderNode &node)
{
  uint data_inputs = 0;
  for (const XInputSocket *xsocket : xnode.inputs()) {
    if (this->is_data_socket(*xsocket)) {
      this->map_sockets(*xsocket, *node.inputs()[data_inputs]);
      data_inputs++;
    }
  }

  uint data_outputs = 0;
  for (const XOutputSocket *xsocket : xnode.outputs()) {
    if (this->is_data_socket(*xsocket)) {
      this->map_sockets(*xsocket, *node.outputs()[data_outputs]);
      data_outputs++;
    }
  }
}

void VTreeMFNetworkBuilder::assert_xnode_is_mapped_correctly(const XNode &xnode) const
{
  UNUSED_VARS_NDEBUG(xnode);
#ifdef DEBUG
  this->assert_data_sockets_are_mapped_correctly(xnode.inputs().cast<const XSocket *>());
  this->assert_data_sockets_are_mapped_correctly(xnode.outputs().cast<const XSocket *>());
#endif
}

void VTreeMFNetworkBuilder::assert_data_sockets_are_mapped_correctly(
    ArrayRef<const XSocket *> xsockets) const
{
  for (const XSocket *xsocket : xsockets) {
    if (this->is_data_socket(*xsocket)) {
      this->assert_xsocket_is_mapped_correctly(*xsocket);
    }
  }
}

void VTreeMFNetworkBuilder::assert_xsocket_is_mapped_correctly(const XSocket &xsocket) const
{
  BLI_assert(this->xsocket_is_mapped(xsocket));
  MFDataType xsocket_type = this->try_get_data_type(xsocket).value();
  UNUSED_VARS_NDEBUG(xsocket_type);

  if (xsocket.is_input()) {
    for (MFBuilderInputSocket *socket : this->lookup_socket(xsocket.as_input())) {
      MFDataType socket_type = socket->data_type();
      BLI_assert(socket_type == xsocket_type);
      UNUSED_VARS_NDEBUG(socket_type);
    }
  }
  else {
    MFBuilderSocket &socket = this->lookup_socket(xsocket.as_output());
    MFDataType socket_type = socket.data_type();
    BLI_assert(socket_type == xsocket_type);
    UNUSED_VARS_NDEBUG(socket_type);
  }
}

bool VTreeMFNetworkBuilder::has_data_sockets(const XNode &xnode) const
{
  for (const XInputSocket *xsocket : xnode.inputs()) {
    if (this->is_data_socket(*xsocket)) {
      return true;
    }
  }
  for (const XOutputSocket *xsocket : xnode.outputs()) {
    if (this->is_data_socket(*xsocket)) {
      return true;
    }
  }
  return false;
}

const CPPType &VTreeMFNetworkBuilder::cpp_type_from_property(const XNode &xnode,
                                                             StringRefNull prop_name) const
{
  char *type_name = RNA_string_get_alloc(xnode.rna(), prop_name.data(), nullptr, 0);
  const CPPType &type = this->cpp_type_by_name(type_name);
  MEM_freeN(type_name);
  return type;
}

MFDataType VTreeMFNetworkBuilder::data_type_from_property(const XNode &xnode,
                                                          StringRefNull prop_name) const
{
  char *type_name = RNA_string_get_alloc(xnode.rna(), prop_name.data(), nullptr, 0);
  MFDataType type = m_inlined_tree_mappings.data_type_by_type_name.lookup(type_name);
  MEM_freeN(type_name);
  return type;
}

Vector<bool> VNodeMFNetworkBuilder::get_list_base_variadic_states(StringRefNull prop_name)
{
  Vector<bool> states;
  RNA_BEGIN (m_xnode.rna(), itemptr, prop_name.data()) {
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
  m_network_builder.map_data_sockets(m_xnode, node);
}

const MultiFunction &VNodeMFNetworkBuilder::get_vectorized_function(
    const MultiFunction &base_function, ArrayRef<const char *> is_vectorized_prop_names)
{
  Vector<bool> input_is_vectorized;
  for (const char *prop_name : is_vectorized_prop_names) {
    char state[5];
    RNA_string_get(m_xnode.rna(), prop_name, state);
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

  Array<uint> xsocket_by_socket(network->socket_ids().size(), VTreeMFSocketMap_UNMAPPED);
  for (uint xsocket_id : m_single_socket_by_xsocket.index_iterator()) {
    switch (m_single_socket_by_xsocket[xsocket_id]) {
      case VTreeMFSocketMap_UNMAPPED: {
        break;
      }
      case VTreeMFSocketMap_MULTIMAPPED: {
        for (uint socket_id : m_multiple_inputs_by_xsocket.lookup(xsocket_id)) {
          xsocket_by_socket[socket_id] = xsocket_id;
        }
        break;
      }
      default: {
        uint socket_id = m_single_socket_by_xsocket[xsocket_id];
        xsocket_by_socket[socket_id] = xsocket_id;
        break;
      }
    }
  }

  VTreeMFSocketMap socket_map(m_inlined_tree,
                              *network,
                              std::move(m_single_socket_by_xsocket),
                              std::move(m_multiple_inputs_by_xsocket),
                              std::move(xsocket_by_socket));

  return BLI::make_unique<VTreeMFNetwork>(
      m_inlined_tree, std::move(network), std::move(socket_map));
}

}  // namespace FN
