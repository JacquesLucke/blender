#pragma once

#include "FN_inlined_tree_multi_function_network.h"
#include "FN_multi_functions.h"

#include "BLI_multi_map.h"

#include "mappings.h"

namespace FN {

using BKE::VSocket;
using BKE::XGroupInput;
using BLI::MultiMap;

class PreprocessedVTreeMFData {
 private:
  const InlinedNodeTree &m_inlined_tree;
  Array<Optional<MFDataType>> m_data_type_by_xsocket_id;
  Array<Optional<MFDataType>> m_data_type_by_group_input_id;

 public:
  PreprocessedVTreeMFData(const InlinedNodeTree &inlined_tree) : m_inlined_tree(inlined_tree)
  {
    auto &mappings = get_inlined_tree_multi_function_mappings();

    m_data_type_by_xsocket_id = Array<Optional<MFDataType>>(inlined_tree.socket_count());
    for (const XSocket *xsocket : inlined_tree.all_sockets()) {
      m_data_type_by_xsocket_id[xsocket->id()] = mappings.data_type_by_idname.try_lookup(
          xsocket->idname());
    }

    m_data_type_by_group_input_id = Array<Optional<MFDataType>>(
        inlined_tree.all_group_inputs().size());
    for (const XGroupInput *group_input : inlined_tree.all_group_inputs()) {
      m_data_type_by_group_input_id[group_input->id()] = mappings.data_type_by_idname.try_lookup(
          group_input->vsocket().idname());
    }
  }

  Optional<MFDataType> try_lookup_data_type(const XSocket &xsocket) const
  {
    return m_data_type_by_xsocket_id[xsocket.id()];
  }

  MFDataType lookup_data_type(const XSocket &xsocket) const
  {
    return m_data_type_by_xsocket_id[xsocket.id()].value();
  }

  bool is_data_socket(const XSocket &xsocket) const
  {
    return m_data_type_by_xsocket_id[xsocket.id()].has_value();
  }

  bool is_data_group_input(const XGroupInput &group_input) const
  {
    return m_data_type_by_group_input_id[group_input.id()].has_value();
  }
};

class VTreeMFNetworkBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  const InlinedNodeTree &m_inlined_tree;
  const PreprocessedVTreeMFData &m_preprocessed_inlined_tree_data;
  const VTreeMultiFunctionMappings &m_inlined_tree_mappings;
  ResourceCollector &m_resources;

  /* By default store mapping between xsockets and builder sockets in an array.
   * Input xsockets can be mapped to multiple new sockets. So fallback to a multimap in this case.
   */
  Array<uint> m_single_socket_by_xsocket;
  MultiMap<uint, uint> m_multiple_inputs_by_xsocket;
  static constexpr intptr_t MULTI_MAP_INDICATOR = 1;

  Map<const XGroupInput *, MFBuilderOutputSocket *> m_group_inputs_mapping;

  std::unique_ptr<MFNetworkBuilder> m_builder;

 public:
  VTreeMFNetworkBuilder(const InlinedNodeTree &inlined_tree,
                        const PreprocessedVTreeMFData &preprocessed_inlined_tree_data,
                        const VTreeMultiFunctionMappings &inlined_tree_mappings,
                        ResourceCollector &resources);

  const InlinedNodeTree &inlined_tree() const
  {
    return m_inlined_tree;
  }

  ResourceCollector &resources()
  {
    return m_resources;
  }

  MFBuilderFunctionNode &add_function(const MultiFunction &function);

  MFBuilderFunctionNode &add_function(const MultiFunction &function, const XNode &xnode);

  MFBuilderDummyNode &add_dummy(const XNode &xnode);

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

  Optional<MFDataType> try_get_data_type(const XSocket &xsocket) const
  {
    return m_preprocessed_inlined_tree_data.try_lookup_data_type(xsocket);
  }

  bool is_data_socket(const XSocket &xsocket) const
  {
    return m_preprocessed_inlined_tree_data.is_data_socket(xsocket);
  }

  bool is_data_group_input(const XGroupInput &group_input) const
  {
    return m_preprocessed_inlined_tree_data.is_data_group_input(group_input);
  }

  void map_data_sockets(const XNode &xnode, MFBuilderNode &node);

  void map_sockets(const XInputSocket &xsocket, MFBuilderInputSocket &socket)
  {
    switch (m_single_socket_by_xsocket[xsocket.id()]) {
      case VTreeMFSocketMap_UNMAPPED: {
        m_single_socket_by_xsocket[xsocket.id()] = socket.id();
        break;
      }
      case VTreeMFSocketMap_MULTIMAPPED: {
        BLI_assert(!m_multiple_inputs_by_xsocket.lookup(xsocket.id()).contains(socket.id()));
        m_multiple_inputs_by_xsocket.add(xsocket.id(), socket.id());
        break;
      }
      default: {
        uint already_inserted_id = m_single_socket_by_xsocket[xsocket.id()];
        BLI_assert(already_inserted_id != socket.id());
        m_multiple_inputs_by_xsocket.add_multiple_new(xsocket.id(),
                                                      {already_inserted_id, socket.id()});
        m_single_socket_by_xsocket[xsocket.id()] = VTreeMFSocketMap_MULTIMAPPED;
        break;
      }
    }
  }

  void map_sockets(const XOutputSocket &xsocket, MFBuilderOutputSocket &socket)
  {
    BLI_assert(m_single_socket_by_xsocket[xsocket.id()] == VTreeMFSocketMap_UNMAPPED);
    m_single_socket_by_xsocket[xsocket.id()] = socket.id();
  }

  void map_sockets(ArrayRef<const XInputSocket *> xsockets,
                   ArrayRef<MFBuilderInputSocket *> sockets)
  {
    BLI_assert(xsockets.size() == sockets.size());
    for (uint i : xsockets.index_iterator()) {
      this->map_sockets(*xsockets[i], *sockets[i]);
    }
  }

  void map_sockets(ArrayRef<const XOutputSocket *> xsockets,
                   ArrayRef<MFBuilderOutputSocket *> sockets)
  {
    BLI_assert(xsockets.size() == sockets.size());
    for (uint i : xsockets.index_iterator()) {
      this->map_sockets(*xsockets[i], *sockets[i]);
    }
  }

  void map_group_input(const XGroupInput &group_input, MFBuilderOutputSocket &socket)
  {
    m_group_inputs_mapping.add_new(&group_input, &socket);
  }

  MFBuilderOutputSocket &lookup_group_input(const XGroupInput &group_input) const
  {
    return *m_group_inputs_mapping.lookup(&group_input);
  }

  bool xsocket_is_mapped(const XSocket &xsocket) const
  {
    return m_single_socket_by_xsocket[xsocket.id()] != VTreeMFSocketMap_UNMAPPED;
  }

  void assert_xnode_is_mapped_correctly(const XNode &xnode) const;
  void assert_data_sockets_are_mapped_correctly(ArrayRef<const XSocket *> xsockets) const;
  void assert_xsocket_is_mapped_correctly(const XSocket &xsocket) const;

  bool has_data_sockets(const XNode &xnode) const;

  MFBuilderSocket &lookup_single_socket(const XSocket &xsocket) const
  {
    uint mapped_id = m_single_socket_by_xsocket[xsocket.id()];
    BLI_assert(!ELEM(mapped_id, VTreeMFSocketMap_MULTIMAPPED, VTreeMFSocketMap_UNMAPPED));
    return *m_builder->sockets_by_id()[mapped_id];
  }

  MFBuilderOutputSocket &lookup_socket(const XOutputSocket &xsocket) const
  {
    return this->lookup_single_socket(xsocket.as_base()).as_output();
  }

  Vector<MFBuilderInputSocket *> lookup_socket(const XInputSocket &xsocket) const
  {
    Vector<MFBuilderInputSocket *> sockets;
    switch (m_single_socket_by_xsocket[xsocket.id()]) {
      case VTreeMFSocketMap_UNMAPPED: {
        break;
      }
      case VTreeMFSocketMap_MULTIMAPPED: {
        for (uint mapped_id : m_multiple_inputs_by_xsocket.lookup(xsocket.id())) {
          sockets.append(&m_builder->sockets_by_id()[mapped_id]->as_input());
        }
        break;
      }
      default: {
        uint mapped_id = m_single_socket_by_xsocket[xsocket.id()];
        sockets.append(&m_builder->sockets_by_id()[mapped_id]->as_input());
        break;
      }
    }
    return sockets;
  }

  const CPPType &cpp_type_by_name(StringRef name) const
  {
    return *m_inlined_tree_mappings.cpp_type_by_type_name.lookup(name);
  }

  const CPPType &cpp_type_from_property(const XNode &xnode, StringRefNull prop_name) const;
  MFDataType data_type_from_property(const XNode &xnode, StringRefNull prop_name) const;

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
  const XNode &m_xnode;

 public:
  VNodeMFNetworkBuilder(VTreeMFNetworkBuilder &network_builder, const XNode &xnode)
      : m_network_builder(network_builder), m_xnode(xnode)
  {
  }

  VTreeMFNetworkBuilder &network_builder()
  {
    return m_network_builder;
  }

  const XNode &xnode() const
  {
    return m_xnode;
  }

  PointerRNA *rna()
  {
    return m_xnode.rna();
  }

  const CPPType &cpp_type_from_property(StringRefNull prop_name)
  {
    return m_network_builder.cpp_type_from_property(m_xnode, prop_name);
  }

  MFDataType data_type_from_property(StringRefNull prop_name)
  {
    return m_network_builder.data_type_from_property(m_xnode, prop_name);
  }

  std::string string_from_property(StringRefNull prop_name)
  {
    char *str_ptr = RNA_string_get_alloc(m_xnode.rna(), prop_name.data(), nullptr, 0);
    std::string str = str_ptr;
    MEM_freeN(str_ptr);
    return str;
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
