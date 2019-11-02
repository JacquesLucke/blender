#pragma once

#include "FN_vtree_multi_function_network.h"

#include "mappings.h"

namespace FN {

class VTreeMFNetworkBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VirtualNodeTree &m_vtree;
  const VTreeMultiFunctionMappings &m_vtree_mappings;
  Array<MFBuilderSocket *> m_socket_map;
  Array<MFDataType> m_type_by_vsocket;
  std::unique_ptr<MFNetworkBuilder> m_builder;

 public:
  VTreeMFNetworkBuilder(const VirtualNodeTree &vtree,
                        const VTreeMultiFunctionMappings &vtree_mappings)
      : m_vtree(vtree),
        m_vtree_mappings(vtree_mappings),
        m_socket_map(vtree.socket_count(), nullptr),
        m_type_by_vsocket(vtree.socket_count()),
        m_builder(BLI::make_unique<MFNetworkBuilder>())
  {
    for (const VSocket *vsocket : vtree.all_sockets()) {
      MFDataType data_type = vtree_mappings.data_type_by_idname.lookup_default(
          vsocket->idname(), MFDataType::ForNone());
      m_type_by_vsocket[vsocket->id()] = data_type;
    }
  }

  const VirtualNodeTree &vtree() const
  {
    return m_vtree;
  }

  MFBuilderFunctionNode &add_function(const MultiFunction &function,
                                      ArrayRef<uint> input_param_indices,
                                      ArrayRef<uint> output_param_indices)
  {
    return m_builder->add_function(function, input_param_indices, output_param_indices);
  }

  MFBuilderFunctionNode &add_function(const MultiFunction &function,
                                      ArrayRef<uint> input_param_indices,
                                      ArrayRef<uint> output_param_indices,
                                      const VNode &vnode)
  {
    MFBuilderFunctionNode &node = m_builder->add_function(
        function, input_param_indices, output_param_indices);
    this->map_sockets_exactly(vnode, node);
    return node;
  }

  MFBuilderDummyNode &add_dummy(const VNode &vnode)
  {
    Vector<MFDataType> input_types;
    for (const VInputSocket *vsocket : vnode.inputs()) {
      MFDataType data_type = this->try_get_data_type(*vsocket);
      if (!data_type.is_none()) {
        input_types.append(data_type);
      }
    }

    Vector<MFDataType> output_types;
    for (const VOutputSocket *vsocket : vnode.outputs()) {
      MFDataType data_type = this->try_get_data_type(*vsocket);
      if (!data_type.is_none()) {
        output_types.append(data_type);
      }
    }

    MFBuilderDummyNode &node = m_builder->add_dummy(input_types, output_types);
    this->map_data_sockets(vnode, node);
    return node;
  }

  MFBuilderDummyNode &add_dummy(ArrayRef<MFDataType> input_types,
                                ArrayRef<MFDataType> output_types)
  {
    return m_builder->add_dummy(input_types, output_types);
  }

  void add_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to)
  {
    m_builder->add_link(from, to);
  }

  MFDataType try_get_data_type(const VSocket &vsocket) const
  {
    return m_type_by_vsocket[vsocket.id()];
  }

  bool is_data_socket(const VSocket &vsocket) const
  {
    return !m_type_by_vsocket[vsocket.id()].is_none();
  }

  void map_sockets_exactly(const VNode &vnode, MFBuilderNode &node)
  {
    BLI_assert(vnode.inputs().size() == node.inputs().size());
    BLI_assert(vnode.outputs().size() == node.outputs().size());

    for (uint i = 0; i < vnode.inputs().size(); i++) {
      m_socket_map[vnode.inputs()[i]->id()] = node.inputs()[i];
    }
    for (uint i = 0; i < vnode.outputs().size(); i++) {
      m_socket_map[vnode.outputs()[i]->id()] = node.outputs()[i];
    }
  }

  void map_data_sockets(const VNode &vnode, MFBuilderNode &node)
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

  void map_sockets(const VInputSocket &vsocket, MFBuilderInputSocket &socket)
  {
    BLI_assert(m_socket_map[vsocket.id()] == nullptr);
    m_socket_map[vsocket.id()] = &socket;
  }

  void map_sockets(const VOutputSocket &vsocket, MFBuilderOutputSocket &socket)
  {
    BLI_assert(m_socket_map[vsocket.id()] == nullptr);
    m_socket_map[vsocket.id()] = &socket;
  }

  bool vsocket_is_mapped(const VSocket &vsocket) const
  {
    return m_socket_map[vsocket.id()] != nullptr;
  }

  bool data_sockets_are_mapped(ArrayRef<const VSocket *> vsockets) const
  {
    for (const VSocket *vsocket : vsockets) {
      if (this->is_data_socket(*vsocket)) {
        if (!this->vsocket_is_mapped(*vsocket)) {
          return false;
        }
      }
    }
    return true;
  }

  bool data_sockets_of_vnode_are_mapped(const VNode &vnode) const
  {
    if (!this->data_sockets_are_mapped(vnode.inputs().cast<const VSocket *>())) {
      return false;
    }
    if (!this->data_sockets_are_mapped(vnode.outputs().cast<const VSocket *>())) {
      return false;
    }
    return true;
  }

  bool has_data_sockets(const VNode &vnode) const
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

  bool is_input_linked(const VInputSocket &vsocket) const
  {
    auto &socket = this->lookup_socket(vsocket);
    return socket.as_input().origin() != nullptr;
  }

  MFBuilderOutputSocket &lookup_socket(const VOutputSocket &vsocket) const
  {
    MFBuilderSocket *socket = m_socket_map[vsocket.id()];
    BLI_assert(socket != nullptr);
    return socket->as_output();
  }

  MFBuilderInputSocket &lookup_socket(const VInputSocket &vsocket) const
  {
    MFBuilderSocket *socket = m_socket_map[vsocket.id()];
    BLI_assert(socket != nullptr);
    return socket->as_input();
  }

  const CPPType &cpp_type_by_name(StringRef name) const
  {
    return *m_vtree_mappings.cpp_type_by_type_name.lookup(name);
  }

  const CPPType &cpp_type_from_property(const VNode &vnode, StringRefNull prop_name) const
  {
    char *type_name = RNA_string_get_alloc(vnode.rna(), prop_name.data(), nullptr, 0);
    const CPPType &type = this->cpp_type_by_name(type_name);
    MEM_freeN(type_name);
    return type;
  }

  std::unique_ptr<VTreeMFNetwork> build()
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
};

}  // namespace FN
