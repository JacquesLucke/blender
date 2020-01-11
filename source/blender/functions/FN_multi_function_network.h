#ifndef __FN_MULTI_FUNCTION_NETWORK_H__
#define __FN_MULTI_FUNCTION_NETWORK_H__

#include "FN_multi_function.h"

#include "BLI_optional.h"
#include "BLI_array_cxx.h"
#include "BLI_set.h"

namespace FN {

using BLI::Array;
using BLI::Optional;
using BLI::Set;

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
  Vector<MFBuilderInputSocket *> m_inputs;
  Vector<MFBuilderOutputSocket *> m_outputs;
  uint m_id;
  bool m_is_dummy;

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
};

class MFBuilderFunctionNode : public MFBuilderNode {
 private:
  const MultiFunction *m_function;
  Vector<uint> m_input_param_indices;
  Vector<uint> m_output_param_indices;

  friend MFNetworkBuilder;

 public:
  const MultiFunction &function();

  ArrayRef<uint> input_param_indices();
  ArrayRef<uint> output_param_indices();
};

class MFBuilderDummyNode : public MFBuilderNode {
 private:
  StringRefNull m_name;
  Vector<StringRefNull> m_input_names;
  Vector<StringRefNull> m_output_names;

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
  uint id();
  StringRefNull name();

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

  Vector<MFBuilderNode *> m_node_by_id;
  Vector<MFBuilderSocket *> m_socket_by_id;

  Vector<MFBuilderFunctionNode *> m_function_nodes;
  Vector<MFBuilderDummyNode *> m_dummy_nodes;
  Vector<MFBuilderInputSocket *> m_input_sockets;
  Vector<MFBuilderOutputSocket *> m_output_sockets;

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
  void add_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to);
  void remove_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to);

  ArrayRef<MFBuilderNode *> all_nodes() const
  {
    return m_node_by_id;
  }

  ArrayRef<MFBuilderNode *> nodes_by_id() const
  {
    return m_node_by_id;
  }

  ArrayRef<MFBuilderSocket *> sockets_by_id() const
  {
    return m_socket_by_id;
  }

  ArrayRef<MFBuilderFunctionNode *> function_nodes() const
  {
    return m_function_nodes;
  }

  ArrayRef<MFBuilderDummyNode *> dummy_nodes() const
  {
    return m_dummy_nodes;
  }

  ArrayRef<MFBuilderInputSocket *> input_sockets() const
  {
    return m_input_sockets;
  }

  ArrayRef<MFBuilderOutputSocket *> output_sockets() const
  {
    return m_output_sockets;
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
  const MFNetwork *m_network;
  Vector<const MFInputSocket *> m_inputs;
  Vector<const MFOutputSocket *> m_outputs;
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
};

class MFFunctionNode final : public MFNode {
 private:
  const MultiFunction *m_function;
  Vector<uint> m_input_param_indices;
  Vector<uint> m_output_param_indices;

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
  Vector<StringRefNull> m_input_names;
  Vector<StringRefNull> m_output_names;

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
};

class MFNetwork : BLI::NonCopyable, BLI::NonMovable {
 private:
  MonotonicAllocator<> m_allocator;

  Array<MFNode *> m_node_by_id;
  Array<MFSocket *> m_socket_by_id;

  Vector<MFFunctionNode *> m_function_nodes;
  Vector<MFDummyNode *> m_dummy_nodes;
  Vector<MFInputSocket *> m_input_sockets;
  Vector<MFOutputSocket *> m_output_sockets;

 public:
  MFNetwork(MFNetworkBuilder &builder);
  ~MFNetwork();

  const MFNode &node_by_id(uint id) const;
  const MFSocket &socket_by_id(uint id) const;
  IndexRange socket_ids() const;

  Vector<const MFOutputSocket *> find_dummy_dependencies(
      ArrayRef<const MFInputSocket *> sockets) const;

  Vector<const MFFunctionNode *> find_function_dependencies(
      ArrayRef<const MFInputSocket *> sockets) const;
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

inline uint MFBuilderNode::id()
{
  return m_id;
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

inline uint MFBuilderSocket::id()
{
  return m_id;
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
  return this->input(m_input_param_indices.index(param_index));
}

inline const MFOutputSocket &MFFunctionNode::output_for_param(uint param_index) const
{
  return this->output(m_output_param_indices.index(param_index));
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

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_NETWORK_H__ */
