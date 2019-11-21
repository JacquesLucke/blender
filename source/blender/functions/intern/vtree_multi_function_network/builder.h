#pragma once

#include "FN_vtree_multi_function_network.h"
#include "FN_multi_functions.h"

#include "BLI_multi_map.h"

#include "mappings.h"

namespace FN {

using BLI::MultiMap;

class PreprocessedVTreeMFData {
 private:
  const VirtualNodeTree &m_vtree;
  Array<Optional<MFDataType>> m_data_type_by_vsocket_id;

 public:
  PreprocessedVTreeMFData(const VirtualNodeTree &vtree) : m_vtree(vtree)
  {
    auto &mappings = get_vtree_multi_function_mappings();

    m_data_type_by_vsocket_id = Array<Optional<MFDataType>>(vtree.socket_count());
    for (const VSocket *vsocket : vtree.all_sockets()) {
      const MFDataType *data_type = mappings.data_type_by_idname.lookup_ptr(vsocket->idname());
      if (data_type == nullptr) {
        m_data_type_by_vsocket_id[vsocket->id()] = {};
      }
      else {
        m_data_type_by_vsocket_id[vsocket->id()] = MFDataType(*data_type);
      }
    }
  }

  Optional<MFDataType> try_lookup_data_type(const VSocket &vsocket) const
  {
    return m_data_type_by_vsocket_id[vsocket.id()];
  }

  MFDataType lookup_data_type(const VSocket &vsocket) const
  {
    return m_data_type_by_vsocket_id[vsocket.id()].value();
  }

  bool is_data_socket(const VSocket &vsocket) const
  {
    return m_data_type_by_vsocket_id[vsocket.id()].has_value();
  }
};

class VTreeMFNetworkBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VirtualNodeTree &m_vtree;
  const PreprocessedVTreeMFData &m_preprocessed_vtree_data;
  const VTreeMultiFunctionMappings &m_vtree_mappings;
  ResourceCollector &m_resources;

  /* By default store mapping between vsockets and builder sockets in an array.
   * Input vsockets can be mapped to multiple new sockets. So fallback to a multimap in this case.
   */
  Array<uint> m_single_socket_by_vsocket;
  MultiMap<uint, uint> m_multiple_inputs_by_vsocket;
  static constexpr intptr_t MULTI_MAP_INDICATOR = 1;

  std::unique_ptr<MFNetworkBuilder> m_builder;

 public:
  VTreeMFNetworkBuilder(const VirtualNodeTree &vtree,
                        const PreprocessedVTreeMFData &preprocessed_vtree_data,
                        const VTreeMultiFunctionMappings &vtree_mappings,
                        ResourceCollector &resources);

  const VirtualNodeTree &vtree() const
  {
    return m_vtree;
  }

  ResourceCollector &resources()
  {
    return m_resources;
  }

  MFBuilderFunctionNode &add_function(const MultiFunction &function);

  MFBuilderFunctionNode &add_function(const MultiFunction &function, const VNode &vnode);

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
    return m_preprocessed_vtree_data.try_lookup_data_type(vsocket);
  }

  bool is_data_socket(const VSocket &vsocket) const
  {
    return m_preprocessed_vtree_data.is_data_socket(vsocket);
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

class VSocketMFNetworkBuilder {
 private:
  VTreeMFNetworkBuilder &m_network_builder;
  const VSocket &m_vsocket;
  MFBuilderOutputSocket *m_socket_to_build = nullptr;

 public:
  VSocketMFNetworkBuilder(VTreeMFNetworkBuilder &network_builder, const VSocket &vsocket)
      : m_network_builder(network_builder), m_vsocket(vsocket)
  {
  }

  MFBuilderOutputSocket &built_socket()
  {
    BLI_assert(m_socket_to_build != nullptr);
    return *m_socket_to_build;
  }

  const VSocket &vsocket() const
  {
    return m_vsocket;
  }

  PointerRNA *rna()
  {
    return m_vsocket.rna();
  }

  VTreeMFNetworkBuilder &network_builder()
  {
    return m_network_builder;
  }

  template<typename T> void set_constant_value(const T &value)
  {
    const MultiFunction &fn = m_network_builder.construct_fn<MF_ConstantValue<T>>(value);
    this->set_generator_fn(fn);
  }

  void set_generator_fn(const MultiFunction &fn)
  {
    MFBuilderFunctionNode &node = m_network_builder.add_function(fn);
    this->set_socket(node.output(0));
  }

  void set_socket(MFBuilderOutputSocket &socket)
  {
    m_socket_to_build = &socket;
  }
};

class VNodeMFNetworkBuilder {
 private:
  VTreeMFNetworkBuilder &m_network_builder;
  const VNode &m_vnode;

 public:
  VNodeMFNetworkBuilder(VTreeMFNetworkBuilder &network_builder, const VNode &vnode)
      : m_network_builder(network_builder), m_vnode(vnode)
  {
  }

  VTreeMFNetworkBuilder &network_builder()
  {
    return m_network_builder;
  }

  const VNode &vnode() const
  {
    return m_vnode;
  }

  PointerRNA *rna()
  {
    return m_vnode.rna();
  }

  const CPPType &cpp_type_from_property(StringRefNull prop_name)
  {
    return m_network_builder.cpp_type_from_property(m_vnode, prop_name);
  }

  MFDataType data_type_from_property(StringRefNull prop_name)
  {
    return m_network_builder.data_type_from_property(m_vnode, prop_name);
  }

  Vector<bool> get_list_base_variadic_states(StringRefNull prop_name);

  template<typename T, typename... Args> T &construct_fn(Args &&... args)
  {
    return m_network_builder.construct_fn<T>(std::forward<Args>(args)...);
  }

  template<typename T, typename... Args>
  void set_vectorized_constructed_matching_fn(ArrayRef<const char *> is_vectorized_prop_names,
                                              Args &&... args)
  {
    const MultiFunction &base_fn = this->construct_fn<T>(std::forward<Args>(args)...);
    const MultiFunction &fn = this->get_vectorized_function(base_fn, is_vectorized_prop_names);
    this->set_matching_fn(fn);
  }

  template<typename T, typename... Args> void set_constructed_matching_fn(Args &&... args)
  {
    const MultiFunction &fn = this->construct_fn<T>(std::forward<Args>(args)...);
    this->set_matching_fn(fn);
  }

  void set_matching_fn(const MultiFunction &fn);

 private:
  const MultiFunction &get_vectorized_function(const MultiFunction &base_function,
                                               ArrayRef<const char *> is_vectorized_prop_names);
};

}  // namespace FN
