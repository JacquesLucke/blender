#pragma once

#include "FN_vtree_multi_function_network.h"

#include "mappings.h"

namespace FN {

class VTreeMFNetworkBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VirtualNodeTree &m_vtree;
  const VTreeMultiFunctionMappings &m_vtree_mappings;
  OwnedResources &m_resources;
  Array<MFBuilderSocket *> m_socket_map;
  Array<MFDataType> m_type_by_vsocket;
  std::unique_ptr<MFNetworkBuilder> m_builder;

 public:
  VTreeMFNetworkBuilder(const VirtualNodeTree &vtree,
                        const VTreeMultiFunctionMappings &vtree_mappings,
                        OwnedResources &resources);

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

  template<typename T, typename... Args> T &allocate(const char *name, Args &&... args)
  {
    std::unique_ptr<T> value = BLI::make_unique<T>(std::forward<Args>(args)...);
    T &value_ref = *value;
    m_resources.add(std::move(value), name);
    return value_ref;
  }

  template<typename T, typename... Args> T &allocate_function(Args &&... args)
  {
    BLI_STATIC_ASSERT((std::is_base_of<MultiFunction, T>::value), "");
    std::unique_ptr<T> function = BLI::make_unique<T>(std::forward<Args>(args)...);
    T &function_ref = *function;
    m_resources.add(std::move(function), function_ref.name().data());
    return function_ref;
  }

  MFDataType try_get_data_type(const VSocket &vsocket) const
  {
    return m_type_by_vsocket[vsocket.id()];
  }

  bool is_data_socket(const VSocket &vsocket) const
  {
    return !m_type_by_vsocket[vsocket.id()].is_none();
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

  std::unique_ptr<VTreeMFNetwork> build();
};

}  // namespace FN
