#pragma once

#include "FN_vtree_multi_function_network.h"

#include "BLI_multi_map.h"

#include "mappings.h"

namespace FN {

using BLI::MultiMap;

class VTreeMFNetworkBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VirtualNodeTree &m_vtree;
  const VTreeMultiFunctionMappings &m_vtree_mappings;
  ResourceCollector &m_resources;

  /* By default store mapping between vsockets and builder sockets in an array.
   * Input vsockets can be mapped to multiple new sockets. So fallback to a multimap in this case.
   */
  Array<uint> m_single_socket_by_vsocket;
  MultiMap<uint, uint> m_multiple_inputs_by_vsocket;
  static constexpr intptr_t MULTI_MAP_INDICATOR = 1;

  Array<Optional<MFDataType>> m_type_by_vsocket;
  std::unique_ptr<MFNetworkBuilder> m_builder;

 public:
  VTreeMFNetworkBuilder(const VirtualNodeTree &vtree,
                        const VTreeMultiFunctionMappings &vtree_mappings,
                        ResourceCollector &resources);

  const VirtualNodeTree &vtree() const
  {
    return m_vtree;
  }

  MFBuilderFunctionNode &add_function(const MultiFunction &function,
                                      ArrayRef<uint> input_param_indices,
                                      ArrayRef<uint> output_param_indices);

  MFBuilderFunctionNode &add_function(const MultiFunction &function,
                                      ArrayRef<uint> input_param_indices,
                                      ArrayRef<uint> output_param_indices,
                                      const VNode &vnode);

  MFBuilderDummyNode &add_dummy(const VNode &vnode);

  MFBuilderDummyNode &add_dummy(ArrayRef<MFDataType> input_types,
                                ArrayRef<MFDataType> output_types)
  {
    return m_builder->add_dummy(input_types, output_types);
  }

  void add_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to)
  {
    m_builder->add_link(from, to);
  }

  template<typename T, typename... Args> T &construct(const char *name, Args &&... args)
  {
    void *buffer = m_resources.allocate(sizeof(T), alignof(T));
    T *value = new (buffer) T(std::forward<Args>(args)...);
    m_resources.add(BLI::destruct_ptr<T>(value), name);
    return *value;
  }

  template<typename T, typename... Args> T &construct_fn(Args &&... args)
  {
    BLI_STATIC_ASSERT((std::is_base_of<MultiFunction, T>::value), "");
    void *buffer = m_resources.allocate(sizeof(T), alignof(T));
    T *fn = new (buffer) T(std::forward<Args>(args)...);
    m_resources.add(BLI::destruct_ptr<T>(fn), fn->name().data());
    return *fn;
  }

  Optional<MFDataType> try_get_data_type(const VSocket &vsocket) const
  {
    return m_type_by_vsocket[vsocket.id()];
  }

  bool is_data_socket(const VSocket &vsocket) const
  {
    return m_type_by_vsocket[vsocket.id()].has_value();
  }

  void map_data_sockets(const VNode &vnode, MFBuilderNode &node);

  void map_sockets(const VInputSocket &vsocket, MFBuilderInputSocket &socket)
  {
    switch (m_single_socket_by_vsocket[vsocket.id()]) {
      case VTreeMFSocketMap_UNMAPPED: {
        m_single_socket_by_vsocket[vsocket.id()] = socket.id();
        break;
      }
      case VTreeMFSocketMap_MULTIMAPPED: {
        BLI_assert(!m_multiple_inputs_by_vsocket.lookup(vsocket.id()).contains(socket.id()));
        m_multiple_inputs_by_vsocket.add(vsocket.id(), socket.id());
        break;
      }
      default: {
        uint already_inserted_id = m_single_socket_by_vsocket[vsocket.id()];
        BLI_assert(already_inserted_id != socket.id());
        m_multiple_inputs_by_vsocket.add_multiple_new(vsocket.id(),
                                                      {already_inserted_id, socket.id()});
        m_single_socket_by_vsocket[vsocket.id()] = VTreeMFSocketMap_MULTIMAPPED;
        break;
      }
    }
  }

  void map_sockets(const VOutputSocket &vsocket, MFBuilderOutputSocket &socket)
  {
    BLI_assert(m_single_socket_by_vsocket[vsocket.id()] == VTreeMFSocketMap_UNMAPPED);
    m_single_socket_by_vsocket[vsocket.id()] = socket.id();
  }

  void map_sockets(ArrayRef<const VInputSocket *> vsockets,
                   ArrayRef<MFBuilderInputSocket *> sockets)
  {
    BLI_assert(vsockets.size() == sockets.size());
    for (uint i = 0; i < vsockets.size(); i++) {
      this->map_sockets(*vsockets[i], *sockets[i]);
    }
  }

  void map_sockets(ArrayRef<const VOutputSocket *> vsockets,
                   ArrayRef<MFBuilderOutputSocket *> sockets)
  {
    BLI_assert(vsockets.size() == sockets.size());
    for (uint i = 0; i < vsockets.size(); i++) {
      this->map_sockets(*vsockets[i], *sockets[i]);
    }
  }

  bool vsocket_is_mapped(const VSocket &vsocket) const
  {
    return m_single_socket_by_vsocket[vsocket.id()] != VTreeMFSocketMap_UNMAPPED;
  }

  void assert_vnode_is_mapped_correctly(const VNode &vnode) const;
  void assert_data_sockets_are_mapped_correctly(ArrayRef<const VSocket *> vsockets) const;
  void assert_vsocket_is_mapped_correctly(const VSocket &vsocket) const;

  bool has_data_sockets(const VNode &vnode) const;

  MFBuilderSocket &lookup_single_socket(const VSocket &vsocket) const
  {
    uint mapped_id = m_single_socket_by_vsocket[vsocket.id()];
    BLI_assert(!ELEM(mapped_id, VTreeMFSocketMap_MULTIMAPPED, VTreeMFSocketMap_UNMAPPED));
    return *m_builder->sockets_by_id()[mapped_id];
  }

  MFBuilderOutputSocket &lookup_socket(const VOutputSocket &vsocket) const
  {
    return this->lookup_single_socket(vsocket.as_base()).as_output();
  }

  Vector<MFBuilderInputSocket *> lookup_socket(const VInputSocket &vsocket) const
  {
    Vector<MFBuilderInputSocket *> sockets;
    switch (m_single_socket_by_vsocket[vsocket.id()]) {
      case VTreeMFSocketMap_UNMAPPED: {
        break;
      }
      case VTreeMFSocketMap_MULTIMAPPED: {
        for (uint mapped_id : m_multiple_inputs_by_vsocket.lookup(vsocket.id())) {
          sockets.append(&m_builder->sockets_by_id()[mapped_id]->as_input());
        }
        break;
      }
      default: {
        uint mapped_id = m_single_socket_by_vsocket[vsocket.id()];
        sockets.append(&m_builder->sockets_by_id()[mapped_id]->as_input());
        break;
      }
    }
    return sockets;
  }

  const CPPType &cpp_type_by_name(StringRef name) const
  {
    return *m_vtree_mappings.cpp_type_by_type_name.lookup(name);
  }

  const CPPType &cpp_type_from_property(const VNode &vnode, StringRefNull prop_name) const;
  MFDataType data_type_from_property(const VNode &vnode, StringRefNull prop_name) const;

  std::unique_ptr<VTreeMFNetwork> build();
};

}  // namespace FN
