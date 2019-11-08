#pragma once

#include "FN_vtree_multi_function_network.h"

#include "mappings.h"

namespace FN {

class VTreeMFNetworkBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VirtualNodeTree &m_vtree;
  const VTreeMultiFunctionMappings &m_vtree_mappings;
  ResourceCollector &m_resources;
  Array<MFBuilderSocket *> m_socket_map;
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

  void assert_vnode_is_mapped_correctly(const VNode &vnode) const;
  void assert_data_sockets_are_mapped_correctly(ArrayRef<const VSocket *> vsockets) const;
  void assert_vsocket_is_mapped_correctly(const VSocket &vsocket) const;

  bool has_data_sockets(const VNode &vnode) const;

  bool is_input_linked(const VInputSocket &vsocket) const
  {
    auto &socket = this->lookup_socket(vsocket);
    return socket.as_input().origin() != nullptr;
  }

  MFBuilderSocket &lookup_socket(const VSocket &vsocket) const
  {
    MFBuilderSocket *socket = m_socket_map[vsocket.id()];
    BLI_assert(socket != nullptr);
    return *socket;
  }

  MFBuilderOutputSocket &lookup_socket(const VOutputSocket &vsocket) const
  {
    return this->lookup_socket(vsocket.as_base()).as_output();
  }

  MFBuilderInputSocket &lookup_socket(const VInputSocket &vsocket) const
  {
    return this->lookup_socket(vsocket.as_base()).as_input();
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
