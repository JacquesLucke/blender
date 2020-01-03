#pragma once

#include "FN_node_tree_multi_function_network.h"
#include "FN_multi_functions.h"

#include "BLI_multi_map.h"

#include "mappings.h"

namespace FN {

using BKE::VSocket;
using BLI::IndexToRefMultiMap;
using BLI::MultiMap;

class PreprocessedVTreeMFData {
 private:
  Array<Optional<MFDataType>> m_data_type_by_fsocket_id;
  Array<Optional<MFDataType>> m_data_type_by_group_input_id;

 public:
  PreprocessedVTreeMFData(const FunctionNodeTree &function_tree)
  {
    auto &mappings = get_function_tree_multi_function_mappings();

    m_data_type_by_fsocket_id = Array<Optional<MFDataType>>(function_tree.socket_count());
    for (const FSocket *fsocket : function_tree.all_sockets()) {
      m_data_type_by_fsocket_id[fsocket->id()] = mappings.data_type_by_idname.try_lookup(
          fsocket->idname());
    }

    m_data_type_by_group_input_id = Array<Optional<MFDataType>>(
        function_tree.all_group_inputs().size());
    for (const FGroupInput *group_input : function_tree.all_group_inputs()) {
      m_data_type_by_group_input_id[group_input->id()] = mappings.data_type_by_idname.try_lookup(
          group_input->vsocket().idname());
    }
  }

  Optional<MFDataType> try_lookup_data_type(const FSocket &fsocket) const
  {
    return m_data_type_by_fsocket_id[fsocket.id()];
  }

  MFDataType lookup_data_type(const FSocket &fsocket) const
  {
    return m_data_type_by_fsocket_id[fsocket.id()].value();
  }

  bool is_data_socket(const FSocket &fsocket) const
  {
    return m_data_type_by_fsocket_id[fsocket.id()].has_value();
  }

  bool is_data_group_input(const FGroupInput &group_input) const
  {
    return m_data_type_by_group_input_id[group_input.id()].has_value();
  }
};

class FunctionTreeMFNetworkBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  const FunctionNodeTree &m_function_tree;
  const PreprocessedVTreeMFData &m_preprocessed_function_tree_data;
  const VTreeMultiFunctionMappings &m_function_tree_mappings;
  ResourceCollector &m_resources;

  IndexToRefMultiMap<MFBuilderSocket> m_sockets_by_fsocket_id;
  IndexToRefMap<MFBuilderOutputSocket> m_socket_by_group_input_id;

  std::unique_ptr<MFNetworkBuilder> m_builder;

 public:
  FunctionTreeMFNetworkBuilder(const FunctionNodeTree &function_tree,
                               const PreprocessedVTreeMFData &preprocessed_function_tree_data,
                               const VTreeMultiFunctionMappings &function_tree_mappings,
                               ResourceCollector &resources);

  const FunctionNodeTree &function_tree() const
  {
    return m_function_tree;
  }

  ResourceCollector &resources()
  {
    return m_resources;
  }

  const VTreeMultiFunctionMappings &vtree_multi_function_mappings() const
  {
    return m_function_tree_mappings;
  }

  MFBuilderFunctionNode &add_function(const MultiFunction &function);

  MFBuilderFunctionNode &add_function(const MultiFunction &function, const FNode &fnode);

  MFBuilderDummyNode &add_dummy(const FNode &fnode);

  MFBuilderDummyNode &add_dummy(StringRef name,
                                ArrayRef<MFDataType> input_types,
                                ArrayRef<MFDataType> output_types,
                                ArrayRef<StringRef> input_names,
                                ArrayRef<StringRef> output_names)
  {
    return m_builder->add_dummy(name, input_types, output_types, input_names, output_names);
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

  Optional<MFDataType> try_get_data_type(const FSocket &fsocket) const
  {
    return m_preprocessed_function_tree_data.try_lookup_data_type(fsocket);
  }

  bool is_data_socket(const FSocket &fsocket) const
  {
    return m_preprocessed_function_tree_data.is_data_socket(fsocket);
  }

  bool is_data_group_input(const FGroupInput &group_input) const
  {
    return m_preprocessed_function_tree_data.is_data_group_input(group_input);
  }

  void map_data_sockets(const FNode &fnode, MFBuilderNode &node);

  void map_sockets(const FInputSocket &fsocket, MFBuilderInputSocket &socket)
  {
    m_sockets_by_fsocket_id.add(fsocket.id(), socket);
  }

  void map_sockets(const FOutputSocket &fsocket, MFBuilderOutputSocket &socket)
  {
    m_sockets_by_fsocket_id.add(fsocket.id(), socket);
  }

  void map_sockets(ArrayRef<const FInputSocket *> fsockets,
                   ArrayRef<MFBuilderInputSocket *> sockets)
  {
    BLI_assert(fsockets.size() == sockets.size());
    for (uint i : fsockets.index_range()) {
      this->map_sockets(*fsockets[i], *sockets[i]);
    }
  }

  void map_sockets(ArrayRef<const FOutputSocket *> fsockets,
                   ArrayRef<MFBuilderOutputSocket *> sockets)
  {
    BLI_assert(fsockets.size() == sockets.size());
    for (uint i : fsockets.index_range()) {
      this->map_sockets(*fsockets[i], *sockets[i]);
    }
  }

  void map_group_input(const FGroupInput &group_input, MFBuilderOutputSocket &socket)
  {
    m_socket_by_group_input_id.add_new(group_input.id(), socket);
  }

  MFBuilderOutputSocket &lookup_group_input(const FGroupInput &group_input)
  {
    return m_socket_by_group_input_id.lookup(group_input.id());
  }

  bool fsocket_is_mapped(const FSocket &fsocket) const
  {
    return m_sockets_by_fsocket_id.contains(fsocket.id());
  }

  void assert_fnode_is_mapped_correctly(const FNode &fnode);
  void assert_data_sockets_are_mapped_correctly(ArrayRef<const FSocket *> fsockets);
  void assert_fsocket_is_mapped_correctly(const FSocket &fsocket);

  bool has_data_sockets(const FNode &fnode) const;

  MFBuilderSocket &lookup_single_socket(const FSocket &fsocket)
  {
    return m_sockets_by_fsocket_id.lookup_single(fsocket.id());
  }

  MFBuilderOutputSocket &lookup_socket(const FOutputSocket &fsocket)
  {
    return this->lookup_single_socket(fsocket.as_base()).as_output();
  }

  ArrayRef<MFBuilderInputSocket *> lookup_socket(const FInputSocket &fsocket)
  {
    return m_sockets_by_fsocket_id.lookup(fsocket.id()).cast<MFBuilderInputSocket *>();
  }

  const CPPType &cpp_type_by_name(StringRef name) const
  {
    return *m_function_tree_mappings.cpp_type_by_type_name.lookup(name);
  }

  const CPPType &cpp_type_from_property(const FNode &fnode, StringRefNull prop_name) const;
  MFDataType data_type_from_property(const FNode &fnode, StringRefNull prop_name) const;

  std::unique_ptr<FunctionTreeMFNetwork> build();
};

class VSocketMFNetworkBuilder {
 private:
  FunctionTreeMFNetworkBuilder &m_network_builder;
  const VSocket &m_vsocket;
  MFBuilderOutputSocket *m_socket_to_build = nullptr;

 public:
  VSocketMFNetworkBuilder(FunctionTreeMFNetworkBuilder &network_builder, const VSocket &vsocket)
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

  FunctionTreeMFNetworkBuilder &network_builder()
  {
    return m_network_builder;
  }

  template<typename T> void set_constant_value(T value)
  {
    const MultiFunction &fn = m_network_builder.construct_fn<MF_ConstantValue<T>>(
        std::move(value));
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

class FNodeMFNetworkBuilder {
 private:
  FunctionTreeMFNetworkBuilder &m_network_builder;
  const FNode &m_fnode;

 public:
  FNodeMFNetworkBuilder(FunctionTreeMFNetworkBuilder &network_builder, const FNode &fnode)
      : m_network_builder(network_builder), m_fnode(fnode)
  {
  }

  FunctionTreeMFNetworkBuilder &network_builder()
  {
    return m_network_builder;
  }

  const FNode &fnode() const
  {
    return m_fnode;
  }

  PointerRNA *rna()
  {
    return m_fnode.rna();
  }

  const CPPType &cpp_type_from_property(StringRefNull prop_name)
  {
    return m_network_builder.cpp_type_from_property(m_fnode, prop_name);
  }

  MFDataType data_type_from_property(StringRefNull prop_name)
  {
    return m_network_builder.data_type_from_property(m_fnode, prop_name);
  }

  std::string string_from_property(StringRefNull prop_name)
  {
    char *str_ptr = RNA_string_get_alloc(m_fnode.rna(), prop_name.data(), nullptr, 0);
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

  const MultiFunction &get_vectorized_function(const MultiFunction &base_function,
                                               ArrayRef<const char *> is_vectorized_prop_names);
};

}  // namespace FN
