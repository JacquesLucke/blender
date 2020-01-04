#pragma once

#include "FN_node_tree_multi_function_network.h"
#include "FN_multi_functions.h"

#include "BLI_multi_map.h"

#include "mappings.h"

namespace FN {
namespace MFGeneration {

using BKE::VSocket;
using BLI::IndexToRefMultiMap;
using BLI::MultiMap;

class FSocketDataTypes {
 private:
  Array<Optional<MFDataType>> m_data_type_by_fsocket_id;
  Array<Optional<MFDataType>> m_data_type_by_group_input_id;

 public:
  FSocketDataTypes(const FunctionTree &function_tree)
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

  bool has_data_sockets(const FNode &fnode) const
  {
    for (const FInputSocket *fsocket : fnode.inputs()) {
      if (this->is_data_socket(*fsocket)) {
        return true;
      }
    }
    for (const FOutputSocket *fsocket : fnode.outputs()) {
      if (this->is_data_socket(*fsocket)) {
        return true;
      }
    }
    return false;
  }
};

class MFSocketByFSocketMapping {
 private:
  IndexToRefMultiMap<MFBuilderSocket> m_sockets_by_fsocket_id;
  IndexToRefMap<MFBuilderOutputSocket> m_socket_by_group_input_id;

 public:
  MFSocketByFSocketMapping(const FunctionTree &function_tree)
      : m_sockets_by_fsocket_id(function_tree.all_sockets().size()),
        m_socket_by_group_input_id(function_tree.all_group_inputs().size())
  {
  }

  const IndexToRefMultiMap<MFBuilderSocket> &sockets_by_fsocket_id() const
  {
    return m_sockets_by_fsocket_id;
  }

  void add(const FInputSocket &fsocket, MFBuilderInputSocket &socket)
  {
    m_sockets_by_fsocket_id.add(fsocket.id(), socket);
  }

  void add(const FOutputSocket &fsocket, MFBuilderOutputSocket &socket)
  {
    m_sockets_by_fsocket_id.add(fsocket.id(), socket);
  }

  void add(ArrayRef<const FInputSocket *> fsockets, ArrayRef<MFBuilderInputSocket *> sockets)
  {
    BLI_assert(fsockets.size() == sockets.size());
    for (uint i : fsockets.index_range()) {
      this->add(*fsockets[i], *sockets[i]);
    }
  }

  void add(ArrayRef<const FOutputSocket *> fsockets, ArrayRef<MFBuilderOutputSocket *> sockets)
  {
    BLI_assert(fsockets.size() == sockets.size());
    for (uint i : fsockets.index_range()) {
      this->add(*fsockets[i], *sockets[i]);
    }
  }

  void add(const FGroupInput &group_input, MFBuilderOutputSocket &socket)
  {
    m_socket_by_group_input_id.add_new(group_input.id(), socket);
  }

  void add(const FNode &fnode, MFBuilderNode &node, const FSocketDataTypes &fsocket_data_types)
  {
    uint data_inputs = 0;
    for (const FInputSocket *fsocket : fnode.inputs()) {
      if (fsocket_data_types.is_data_socket(*fsocket)) {
        this->add(*fsocket, *node.inputs()[data_inputs]);
        data_inputs++;
      }
    }

    uint data_outputs = 0;
    for (const FOutputSocket *fsocket : fnode.outputs()) {
      if (fsocket_data_types.is_data_socket(*fsocket)) {
        this->add(*fsocket, *node.outputs()[data_outputs]);
        data_outputs++;
      }
    }
  }

  MFBuilderOutputSocket &lookup(const FGroupInput &group_input)
  {
    return m_socket_by_group_input_id.lookup(group_input.id());
  }

  MFBuilderOutputSocket &lookup(const FOutputSocket &fsocket)
  {
    return m_sockets_by_fsocket_id.lookup_single(fsocket.id()).as_output();
  }

  ArrayRef<MFBuilderInputSocket *> lookup(const FInputSocket &fsocket)
  {
    return m_sockets_by_fsocket_id.lookup(fsocket.id()).cast<MFBuilderInputSocket *>();
  }

  bool is_mapped(const FSocket &fsocket) const
  {
    return m_sockets_by_fsocket_id.contains(fsocket.id());
  }

  Vector<std::pair<uint, uint>> get_dummy_mappings()
  {
    Vector<std::pair<uint, uint>> dummy_mappings;
    for (uint fsocket_id : IndexRange(m_sockets_by_fsocket_id.max_index())) {
      ArrayRef<MFBuilderSocket *> mapped_sockets = m_sockets_by_fsocket_id.lookup(fsocket_id);
      if (mapped_sockets.size() == 1) {
        MFBuilderSocket &socket = *mapped_sockets[0];
        if (socket.node().is_dummy()) {
          dummy_mappings.append({fsocket_id, socket.id()});
        }
      }
    }
    return dummy_mappings;
  }
};

struct CommonBuilderData {
  ResourceCollector &resources;
  const VTreeMultiFunctionMappings &mappings;
  const FSocketDataTypes &fsocket_data_types;
  MFSocketByFSocketMapping &socket_map;
  MFNetworkBuilder &network_builder;
  const FunctionTree &function_tree;
};

class CommonBuilderBase {
 protected:
  CommonBuilderData &m_common;

 public:
  CommonBuilderBase(CommonBuilderData &common) : m_common(common)
  {
  }

  CommonBuilderData &common()
  {
    return m_common;
  }

  const FunctionTree &function_tree() const
  {
    return m_common.function_tree;
  }

  ResourceCollector &resources()
  {
    return m_common.resources;
  }

  const VTreeMultiFunctionMappings &mappings() const
  {
    return m_common.mappings;
  }

  MFSocketByFSocketMapping &socket_map() const
  {
    return m_common.socket_map;
  }

  const FSocketDataTypes &fsocket_data_types() const
  {
    return m_common.fsocket_data_types;
  }

  template<typename T, typename... Args> T &construct_fn(Args &&... args)
  {
    BLI_STATIC_ASSERT((std::is_base_of<MultiFunction, T>::value), "");
    void *buffer = m_common.resources.allocate(sizeof(T), alignof(T));
    T *fn = new (buffer) T(std::forward<Args>(args)...);
    m_common.resources.add(BLI::destruct_ptr<T>(fn), fn->name().data());
    return *fn;
  }

  const CPPType &cpp_type_by_name(StringRef name) const
  {
    return *m_common.mappings.cpp_type_by_type_name.lookup(name);
  }

  const CPPType &cpp_type_from_property(const FNode &fnode, StringRefNull prop_name) const
  {
    char *type_name = RNA_string_get_alloc(fnode.rna(), prop_name.data(), nullptr, 0);
    const CPPType &type = this->cpp_type_by_name(type_name);
    MEM_freeN(type_name);
    return type;
  }

  MFDataType data_type_from_property(const FNode &fnode, StringRefNull prop_name) const
  {
    char *type_name = RNA_string_get_alloc(fnode.rna(), prop_name.data(), nullptr, 0);
    MFDataType type = m_common.mappings.data_type_by_type_name.lookup(type_name);
    MEM_freeN(type_name);
    return type;
  }

  void add_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to)
  {
    m_common.network_builder.add_link(from, to);
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
    return m_common.network_builder.add_dummy(
        name, input_types, output_types, input_names, output_names);
  }
};

class VSocketMFNetworkBuilder : public CommonBuilderBase {
 private:
  const VSocket &m_vsocket;
  MFBuilderOutputSocket *m_socket_to_build = nullptr;

 public:
  VSocketMFNetworkBuilder(CommonBuilderData &common, const VSocket &vsocket)
      : CommonBuilderBase(common), m_vsocket(vsocket)
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

  template<typename T> void set_constant_value(T value)
  {
    const MultiFunction &fn = this->construct_fn<MF_ConstantValue<T>>(std::move(value));
    this->set_generator_fn(fn);
  }

  void set_generator_fn(const MultiFunction &fn)
  {
    MFBuilderFunctionNode &node = m_common.network_builder.add_function(fn);
    this->set_socket(node.output(0));
  }

  void set_socket(MFBuilderOutputSocket &socket)
  {
    m_socket_to_build = &socket;
  }
};

class FNodeMFNetworkBuilder : public CommonBuilderBase {
 private:
  const FNode &m_fnode;

  using CommonBuilderBase::cpp_type_from_property;
  using CommonBuilderBase::data_type_from_property;

 public:
  FNodeMFNetworkBuilder(CommonBuilderData &common, const FNode &fnode)
      : CommonBuilderBase(common), m_fnode(fnode)
  {
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
    return this->cpp_type_from_property(m_fnode, prop_name);
  }

  MFDataType data_type_from_property(StringRefNull prop_name)
  {
    return this->data_type_from_property(m_fnode, prop_name);
  }

  std::string string_from_property(StringRefNull prop_name)
  {
    char *str_ptr = RNA_string_get_alloc(m_fnode.rna(), prop_name.data(), nullptr, 0);
    std::string str = str_ptr;
    MEM_freeN(str_ptr);
    return str;
  }

  Vector<bool> get_list_base_variadic_states(StringRefNull prop_name);

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

class ImplicitConversionMFBuilder : public CommonBuilderBase {
 private:
  MFBuilderInputSocket *m_built_input = nullptr;
  MFBuilderOutputSocket *m_built_output = nullptr;

 public:
  ImplicitConversionMFBuilder(CommonBuilderData &common) : CommonBuilderBase(common)
  {
  }

  template<typename T, typename... Args> void set_constructed_conversion_fn(Args &&... args)
  {
    const MultiFunction &fn = this->construct_fn<T>(std::forward<Args>(args)...);
    MFBuilderFunctionNode &node = this->add_function(fn);
    BLI_assert(node.inputs().size() == 1);
    BLI_assert(node.outputs().size() == 1);
    m_built_input = &node.input(0);
    m_built_output = &node.output(0);
  }

  MFBuilderInputSocket &built_input()
  {
    BLI_assert(m_built_input != nullptr);
    return *m_built_input;
  }

  MFBuilderOutputSocket &built_output()
  {
    BLI_assert(m_built_output != nullptr);
    return *m_built_output;
  }
};

}  // namespace MFGeneration
}  // namespace FN
