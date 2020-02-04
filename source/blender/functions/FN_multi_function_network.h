#ifndef __FN_MULTI_FUNCTION_NETWORK_H__
#define __FN_MULTI_FUNCTION_NETWORK_H__

#include "FN_multi_function.h"

#include "BLI_optional.h"
#include "BLI_array_cxx.h"
#include "BLI_set.h"
#include "BLI_vector_set.h"
#include "BLI_map.h"

namespace FN {

using BLI::Array;
using BLI::Map;
using BLI::Optional;
using BLI::Set;
using BLI::VectorSet;

/* MFNetwork Builder
 ****************************************/

class MFBuilderNode;
class MFBuilderFunctionNode;
class MFBuilderDummyNode;

class MFBuilderSocket;
class MFBuilderInputSocket;
class MFBuilderOutputSocket;

class MFNetworkBuilder;

class MFBuilderNode : BLI::NonCopyable, BLI::NonMovable {
 protected:
  MFNetworkBuilder *m_network;
  ArrayRef<MFBuilderInputSocket *> m_inputs;
  ArrayRef<MFBuilderOutputSocket *> m_outputs;
  bool m_is_dummy;
  uint m_id;

  friend MFNetworkBuilder;

 public:
  MFNetworkBuilder &network();

  ArrayRef<MFBuilderInputSocket *> inputs();
  ArrayRef<MFBuilderOutputSocket *> outputs();

  MFBuilderInputSocket &input(uint index);
  MFBuilderOutputSocket &output(uint index);

  StringRefNull name();
  uint id();

  bool is_function();
  bool is_dummy();

  MFBuilderFunctionNode &as_function();
  MFBuilderDummyNode &as_dummy();

  template<typename FuncT> void foreach_target_socket(const FuncT &func);
  template<typename FuncT> void foreach_target_node(const FuncT &func);
  template<typename FuncT> void foreach_origin_node(const FuncT &func);
  template<typename FuncT> void foreach_linked_node(const FuncT &func);
};

class MFBuilderFunctionNode : public MFBuilderNode {
 private:
  const MultiFunction *m_function;
  ArrayRef<uint> m_input_param_indices;
  ArrayRef<uint> m_output_param_indices;

  friend MFNetworkBuilder;

 public:
  const MultiFunction &function();

  ArrayRef<uint> input_param_indices();
  ArrayRef<uint> output_param_indices();
};

class MFBuilderDummyNode : public MFBuilderNode {
 private:
  StringRefNull m_name;
  MutableArrayRef<StringRefNull> m_input_names;
  MutableArrayRef<StringRefNull> m_output_names;

  friend MFNetworkBuilder;
  friend MFBuilderSocket;
  friend MFBuilderNode;
};

class MFBuilderSocket : BLI::NonCopyable, BLI::NonMovable {
 private:
  MFBuilderNode *m_node;
  bool m_is_output;
  uint m_index;
  MFDataType m_data_type;
  uint m_id;

  friend MFNetworkBuilder;

 public:
  MFBuilderNode &node();
  MFDataType data_type();

  uint index();
  StringRefNull name();
  uint id();

  bool is_input();
  bool is_output();

  MFBuilderInputSocket &as_input();
  MFBuilderOutputSocket &as_output();
};

class MFBuilderInputSocket : public MFBuilderSocket {
 private:
  MFBuilderOutputSocket *m_origin;

  friend MFNetworkBuilder;

 public:
  MFBuilderOutputSocket *origin();
};

class MFBuilderOutputSocket : public MFBuilderSocket {
 private:
  Vector<MFBuilderInputSocket *> m_targets;

  friend MFNetworkBuilder;

 public:
  ArrayRef<MFBuilderInputSocket *> targets();
};

class MFNetworkBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  MonotonicAllocator<> m_allocator;

  VectorSet<MFBuilderFunctionNode *> m_function_nodes;
  VectorSet<MFBuilderDummyNode *> m_dummy_nodes;

  Vector<MFBuilderNode *> m_node_or_null_by_id;
  Vector<MFBuilderSocket *> m_socket_or_null_by_id;

 public:
  ~MFNetworkBuilder();

  std::string to_dot(const Set<MFBuilderNode *> &marked_nodes = {});
  void to_dot__clipboard(const Set<MFBuilderNode *> &marked_nodes = {});

  MFBuilderFunctionNode &add_function(const MultiFunction &function);
  MFBuilderDummyNode &add_dummy(StringRef name,
                                ArrayRef<MFDataType> input_types,
                                ArrayRef<MFDataType> output_types,
                                ArrayRef<StringRef> input_names,
                                ArrayRef<StringRef> output_names);
  MFBuilderDummyNode &add_input_dummy(StringRef name, MFBuilderInputSocket &socket);
  MFBuilderDummyNode &add_output_dummy(StringRef name, MFBuilderOutputSocket &socket);

  void add_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to);
  void remove_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to);
  void remove_node(MFBuilderNode &node);
  void remove_nodes(ArrayRef<MFBuilderNode *> nodes);
  void relink_origin(MFBuilderOutputSocket &new_from, MFBuilderInputSocket &to);

  Array<bool> find_nodes_to_the_right_of__inclusive__mask(ArrayRef<MFBuilderNode *> nodes);
  Array<bool> find_nodes_to_the_left_of__inclusive__mask(ArrayRef<MFBuilderNode *> nodes);
  Vector<MFBuilderNode *> find_nodes_not_to_the_left_of__exclusive__vector(
      ArrayRef<MFBuilderNode *> nodes);

  Vector<MFBuilderNode *> nodes_by_id_inverted_id_mask(ArrayRef<bool> id_mask);

  uint current_index_of(MFBuilderFunctionNode &node) const
  {
    return m_function_nodes.index(&node);
  }

  uint current_index_of(MFBuilderDummyNode &node) const
  {
    return m_dummy_nodes.index(&node);
  }

  uint node_id_amount() const
  {
    return m_node_or_null_by_id.size();
  }

  bool node_id_is_valid(uint id) const
  {
    return m_node_or_null_by_id[id] != nullptr;
  }

  MFBuilderNode &node_by_id(uint id)
  {
    BLI_assert(this->node_id_is_valid(id));
    return *m_node_or_null_by_id[id];
  }

  MFBuilderFunctionNode &function_by_id(uint id)
  {
    return this->node_by_id(id).as_function();
  }

  MFBuilderDummyNode &dummy_by_id(uint id)
  {
    return this->node_by_id(id).as_dummy();
  }

  uint socket_id_amount()
  {
    return m_socket_or_null_by_id.size();
  }

  bool socket_id_is_valid(uint id) const
  {
    return m_socket_or_null_by_id[id] != nullptr;
  }

  MFBuilderSocket &socket_by_id(uint id)
  {
    BLI_assert(m_socket_or_null_by_id[id] != nullptr);
    return *m_socket_or_null_by_id[id];
  }

  MFBuilderInputSocket &input_by_id(uint id)
  {
    return this->socket_by_id(id).as_input();
  }

  MFBuilderOutputSocket &output_by_id(uint id)
  {
    return this->socket_by_id(id).as_output();
  }

  ArrayRef<MFBuilderFunctionNode *> function_nodes() const
  {
    return m_function_nodes;
  }

  ArrayRef<MFBuilderDummyNode *> dummy_nodes() const
  {
    return m_dummy_nodes;
  }
};

void optimize_multi_function_network(MFNetworkBuilder &network);

/* Network
 ******************************************/

class MFNode;
class MFFunctionNode;
class MFDummyNode;

class MFSocket;
class MFInputSocket;
class MFOutputSocket;

class MFNetwork;

class MFNode : BLI::NonCopyable, BLI::NonMovable {
 private:
  MFNetwork *m_network;
  ArrayRef<MFInputSocket *> m_inputs;
  ArrayRef<MFOutputSocket *> m_outputs;
  bool m_is_dummy;
  uint m_id;

  friend MFNetwork;

 public:
  const MFNetwork &network() const;

  StringRefNull name() const;

  const MFInputSocket &input(uint index) const;
  const MFOutputSocket &output(uint index) const;

  ArrayRef<const MFInputSocket *> inputs() const;
  ArrayRef<const MFOutputSocket *> outputs() const;

  uint id() const;

  bool is_function() const;
  bool is_dummy() const;

  const MFFunctionNode &as_function() const;
  const MFDummyNode &as_dummy() const;

  template<typename FuncT> void foreach_origin_node(const FuncT &func) const;
  template<typename FuncT> void foreach_origin_socket(const FuncT &func) const;
};

class MFFunctionNode final : public MFNode {
 private:
  const MultiFunction *m_function;
  ArrayRef<uint> m_input_param_indices;
  ArrayRef<uint> m_output_param_indices;

  friend MFNetwork;

 public:
  const MultiFunction &function() const;

  ArrayRef<uint> input_param_indices() const;
  ArrayRef<uint> output_param_indices() const;

  const MFInputSocket &input_for_param(uint param_index) const;
  const MFOutputSocket &output_for_param(uint param_index) const;
};

class MFDummyNode final : public MFNode {
 private:
  StringRefNull m_name;
  MutableArrayRef<StringRefNull> m_input_names;
  MutableArrayRef<StringRefNull> m_output_names;

  friend MFNetwork;
};

class MFSocket : BLI::NonCopyable, BLI::NonMovable {
 private:
  MFNode *m_node;
  bool m_is_output;
  uint m_index;
  MFDataType m_data_type;
  uint m_id;

  friend MFNetwork;

 public:
  const MFNode &node() const;
  MFDataType data_type() const;
  uint param_index() const;
  MFParamType param_type() const;

  uint index() const;
  uint id() const;

  bool is_input() const;
  bool is_output() const;

  MFInputSocket &as_input();
  MFOutputSocket &as_output();

  const MFInputSocket &as_input() const;
  const MFOutputSocket &as_output() const;
};

class MFInputSocket final : public MFSocket {
 private:
  MFOutputSocket *m_origin;

  friend MFNetwork;

 public:
  const MFOutputSocket &origin() const;
};

class MFOutputSocket final : public MFSocket {
 private:
  Vector<const MFInputSocket *> m_targets;

  friend MFNetwork;

 public:
  ArrayRef<const MFInputSocket *> targets() const;
  uint target_amount() const;
};

class MFNetwork : BLI::NonCopyable, BLI::NonMovable {
 private:
  MonotonicAllocator<> m_allocator;

  Vector<MFNode *> m_node_by_id;
  Vector<MFSocket *> m_socket_by_id;

  Vector<MFFunctionNode *> m_function_nodes;
  Vector<MFDummyNode *> m_dummy_nodes;
  Vector<MFInputSocket *> m_input_sockets;
  Vector<MFOutputSocket *> m_output_sockets;

  Array<uint> m_max_dependency_depth_per_node;

 public:
  MFNetwork(MFNetworkBuilder &builder);
  ~MFNetwork();

  const MFNode &node_by_id(uint id) const;
  const MFSocket &socket_by_id(uint id) const;
  IndexRange socket_ids() const;
  IndexRange node_ids() const;

  ArrayRef<const MFDummyNode *> dummy_nodes() const;
  ArrayRef<const MFFunctionNode *> function_nodes() const;

  Vector<const MFOutputSocket *> find_dummy_dependencies(
      ArrayRef<const MFInputSocket *> sockets) const;

  Vector<const MFFunctionNode *> find_function_dependencies(
      ArrayRef<const MFInputSocket *> sockets) const;

  ArrayRef<uint> max_dependency_depth_per_node() const;

  const MFDummyNode &find_dummy_node(MFBuilderDummyNode &builder_node) const;
  const MFInputSocket &find_dummy_socket(MFBuilderInputSocket &builder_socket) const;
  const MFOutputSocket &find_dummy_socket(MFBuilderOutputSocket &builder_socket) const;

 private:
  void create_links_to_node(MFNetworkBuilder &builder,
                            MFNode *to_node,
                            MFBuilderNode *to_builder_node);

  void create_link_to_socket(MFNetworkBuilder &builder,
                             MFInputSocket *to_socket,
                             MFBuilderInputSocket *to_builder_socket);

  void compute_max_dependency_depths();
};

/* Builder Implementations
 *******************************************/

inline MFNetworkBuilder &MFBuilderNode::network()
{
  return *m_network;
}

inline ArrayRef<MFBuilderInputSocket *> MFBuilderNode::inputs()
{
  return m_inputs;
}
inline ArrayRef<MFBuilderOutputSocket *> MFBuilderNode::outputs()
{
  return m_outputs;
}

inline MFBuilderInputSocket &MFBuilderNode::input(uint index)
{
  return *m_inputs[index];
}

inline MFBuilderOutputSocket &MFBuilderNode::output(uint index)
{
  return *m_outputs[index];
}

inline StringRefNull MFBuilderNode::name()
{
  if (this->is_function()) {
    return this->as_function().function().name();
  }
  else {
    return this->as_dummy().m_name;
  }
}

inline uint MFBuilderNode::id()
{
  return m_id;
}

inline bool MFBuilderNode::is_function()
{
  return !m_is_dummy;
}
inline bool MFBuilderNode::is_dummy()
{
  return m_is_dummy;
}

inline MFBuilderFunctionNode &MFBuilderNode::as_function()
{
  BLI_assert(this->is_function());
  return *(MFBuilderFunctionNode *)this;
}

inline MFBuilderDummyNode &MFBuilderNode::as_dummy()
{
  BLI_assert(this->is_dummy());
  return *(MFBuilderDummyNode *)this;
}

template<typename FuncT> inline void MFBuilderNode::foreach_target_socket(const FuncT &func)
{
  for (MFBuilderOutputSocket *socket : m_outputs) {
    for (MFBuilderInputSocket *target : socket->targets()) {
      func(*target);
    }
  }
}

template<typename FuncT> inline void MFBuilderNode::foreach_target_node(const FuncT &func)
{
  for (MFBuilderOutputSocket *socket : m_outputs) {
    for (MFBuilderInputSocket *target : socket->targets()) {
      func(target->node());
    }
  }
}

template<typename FuncT> inline void MFBuilderNode::foreach_origin_node(const FuncT &func)
{
  for (MFBuilderInputSocket *socket : m_inputs) {
    MFBuilderOutputSocket *origin = socket->origin();
    if (origin != nullptr) {
      func(origin->node());
    }
  }
}

template<typename FuncT> inline void MFBuilderNode::foreach_linked_node(const FuncT &func)
{
  this->foreach_origin_node(func);
  this->foreach_target_node(func);
}

inline const MultiFunction &MFBuilderFunctionNode::function()
{
  return *m_function;
}

inline ArrayRef<uint> MFBuilderFunctionNode::input_param_indices()
{
  return m_input_param_indices;
}

inline ArrayRef<uint> MFBuilderFunctionNode::output_param_indices()
{
  return m_output_param_indices;
}

inline MFBuilderNode &MFBuilderSocket::node()
{
  return *m_node;
}

inline MFDataType MFBuilderSocket::data_type()
{
  return m_data_type;
}

inline uint MFBuilderSocket::index()
{
  return m_index;
}

inline StringRefNull MFBuilderSocket::name()
{
  if (m_node->is_function()) {
    MFBuilderFunctionNode &node = m_node->as_function();
    if (m_is_output) {
      return node.function().param_name(node.output_param_indices()[m_index]);
    }
    else {
      return node.function().param_name(node.input_param_indices()[m_index]);
    }
  }
  else {
    MFBuilderDummyNode &node = m_node->as_dummy();
    if (m_is_output) {
      return node.m_output_names[m_index];
    }
    else {
      return node.m_input_names[m_index];
    }
  }
}

inline uint MFBuilderSocket::id()
{
  return m_id;
}

inline bool MFBuilderSocket::is_input()
{
  return !m_is_output;
}
inline bool MFBuilderSocket::is_output()
{
  return m_is_output;
}

inline MFBuilderInputSocket &MFBuilderSocket::as_input()
{
  BLI_assert(this->is_input());
  return *(MFBuilderInputSocket *)this;
}
inline MFBuilderOutputSocket &MFBuilderSocket::as_output()
{
  BLI_assert(this->is_output());
  return *(MFBuilderOutputSocket *)this;
}

inline MFBuilderOutputSocket *MFBuilderInputSocket::origin()
{
  return m_origin;
}

inline ArrayRef<MFBuilderInputSocket *> MFBuilderOutputSocket::targets()
{
  return m_targets;
}

/* MFNetwork Implementations
 **************************************/

inline const MFNetwork &MFNode::network() const
{
  return *m_network;
}

inline ArrayRef<const MFInputSocket *> MFNode::inputs() const
{
  return m_inputs;
}

inline ArrayRef<const MFOutputSocket *> MFNode::outputs() const
{
  return m_outputs;
}

inline const MFInputSocket &MFNode::input(uint index) const
{
  return *m_inputs[index];
}

inline const MFOutputSocket &MFNode::output(uint index) const
{
  return *m_outputs[index];
}

inline uint MFNode::id() const
{
  return m_id;
}

inline StringRefNull MFNode::name() const
{
  if (this->is_function()) {
    return this->as_function().function().name();
  }
  else {
    return "Dummy";
  }
}

inline bool MFNode::is_function() const
{
  return !m_is_dummy;
}

inline bool MFNode::is_dummy() const
{
  return m_is_dummy;
}

inline const MFFunctionNode &MFNode::as_function() const
{
  BLI_assert(this->is_function());
  return *(MFFunctionNode *)this;
}

inline const MFDummyNode &MFNode::as_dummy() const
{
  BLI_assert(this->is_dummy());
  return *(const MFDummyNode *)this;
}

template<typename FuncT> inline void MFNode::foreach_origin_node(const FuncT &func) const
{
  for (const MFInputSocket *socket : m_inputs) {
    const MFOutputSocket &origin_socket = socket->origin();
    const MFNode &origin_node = origin_socket.node();
    func(origin_node);
  }
}

template<typename FuncT> inline void MFNode::foreach_origin_socket(const FuncT &func) const
{
  for (const MFInputSocket *socket : m_inputs) {
    const MFOutputSocket &origin_socket = socket->origin();
    func(origin_socket);
  }
}

inline const MultiFunction &MFFunctionNode::function() const
{
  return *m_function;
}

inline ArrayRef<uint> MFFunctionNode::input_param_indices() const
{
  return m_input_param_indices;
}

inline ArrayRef<uint> MFFunctionNode::output_param_indices() const
{
  return m_output_param_indices;
}

inline const MFInputSocket &MFFunctionNode::input_for_param(uint param_index) const
{
  return this->input(m_input_param_indices.first_index(param_index));
}

inline const MFOutputSocket &MFFunctionNode::output_for_param(uint param_index) const
{
  return this->output(m_output_param_indices.first_index(param_index));
}

inline const MFNode &MFSocket::node() const
{
  return *m_node;
}

inline MFDataType MFSocket::data_type() const
{
  return m_data_type;
}

inline uint MFSocket::param_index() const
{
  const MFFunctionNode &node = m_node->as_function();
  if (m_is_output) {
    return node.output_param_indices()[m_index];
  }
  else {
    return node.input_param_indices()[m_index];
  }
}

inline MFParamType MFSocket::param_type() const
{
  uint param_index = this->param_index();
  return m_node->as_function().function().param_type(param_index);
}

inline uint MFSocket::index() const
{
  return m_index;
}

inline uint MFSocket::id() const
{
  return m_id;
}

inline bool MFSocket::is_input() const
{
  return !m_is_output;
}

inline bool MFSocket::is_output() const
{
  return m_is_output;
}

inline MFInputSocket &MFSocket::as_input()
{
  BLI_assert(this->is_input());
  return *(MFInputSocket *)this;
}

inline MFOutputSocket &MFSocket::as_output()
{
  BLI_assert(this->is_output());
  return *(MFOutputSocket *)this;
}

inline const MFInputSocket &MFSocket::as_input() const
{
  BLI_assert(this->is_input());
  return *(const MFInputSocket *)this;
}

inline const MFOutputSocket &MFSocket::as_output() const
{
  BLI_assert(this->is_output());
  return *(const MFOutputSocket *)this;
}

inline const MFOutputSocket &MFInputSocket::origin() const
{
  return *m_origin;
}

inline ArrayRef<const MFInputSocket *> MFOutputSocket::targets() const
{
  return m_targets;
}

inline uint MFOutputSocket::target_amount() const
{
  return m_targets.size();
}

inline const MFNode &MFNetwork::node_by_id(uint index) const
{
  return *m_node_by_id[index];
}

inline const MFSocket &MFNetwork::socket_by_id(uint index) const
{
  return *m_socket_by_id[index];
}

inline IndexRange MFNetwork::socket_ids() const
{
  return IndexRange(m_socket_by_id.size());
}

inline IndexRange MFNetwork::node_ids() const
{
  return IndexRange(m_node_by_id.size());
}

inline ArrayRef<const MFDummyNode *> MFNetwork::dummy_nodes() const
{
  return m_dummy_nodes.as_ref();
}

inline ArrayRef<const MFFunctionNode *> MFNetwork::function_nodes() const
{
  return m_function_nodes.as_ref();
}

inline ArrayRef<uint> MFNetwork::max_dependency_depth_per_node() const
{
  return m_max_dependency_depth_per_node;
}

inline const MFDummyNode &MFNetwork::find_dummy_node(MFBuilderDummyNode &builder_node) const
{
  uint node_index = builder_node.network().current_index_of(builder_node);
  const MFDummyNode &node = *this->m_dummy_nodes[node_index];
  return node;
}

inline const MFInputSocket &MFNetwork::find_dummy_socket(
    MFBuilderInputSocket &builder_socket) const
{
  const MFDummyNode &node = this->find_dummy_node(builder_socket.node().as_dummy());
  const MFInputSocket &socket = node.input(builder_socket.index());
  return socket;
}

inline const MFOutputSocket &MFNetwork::find_dummy_socket(
    MFBuilderOutputSocket &builder_socket) const
{
  const MFDummyNode &node = this->find_dummy_node(builder_socket.node().as_dummy());
  const MFOutputSocket &socket = node.output(builder_socket.index());
  return socket;
}

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_NETWORK_H__ */
