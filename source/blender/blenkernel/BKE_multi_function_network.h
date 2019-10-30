#ifndef __BKE_MULTI_FUNCTION_NETWORK_H__
#define __BKE_MULTI_FUNCTION_NETWORK_H__

#include "BKE_multi_function.h"

#include "BLI_optional.h"
#include "BLI_array_cxx.h"

namespace BKE {

using BLI::Array;
using BLI::Optional;

/* MFNetwork Builder
 ****************************************/

class MFBuilderNode;
class MFBuilderFunctionNode;
class MFBuilderPlaceholderNode;

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
  bool m_is_placeholder;

  friend MFNetworkBuilder;

 public:
  MFNetworkBuilder &network();

  ArrayRef<MFBuilderInputSocket *> inputs();
  ArrayRef<MFBuilderOutputSocket *> outputs();

  uint id();

  bool is_function();
  bool is_placeholder();

  MFBuilderFunctionNode &as_function();
  MFBuilderPlaceholderNode &as_placeholder();
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

class MFBuilderPlaceholderNode : public MFBuilderNode {
};

class MFBuilderSocket : BLI::NonCopyable, BLI::NonMovable {
 private:
  MFBuilderNode *m_node;
  bool m_is_output;
  uint m_index;
  MFDataType m_type;
  uint m_id;

  friend MFNetworkBuilder;

 public:
  MFBuilderNode &node();
  MFDataType type();

  uint index();
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
  Vector<MFBuilderNode *> m_node_by_id;
  Vector<MFBuilderSocket *> m_socket_by_id;

  Vector<MFBuilderFunctionNode *> m_function_nodes;
  Vector<MFBuilderPlaceholderNode *> m_placeholder_nodes;
  Vector<MFBuilderInputSocket *> m_input_sockets;
  Vector<MFBuilderOutputSocket *> m_output_sockets;

 public:
  ~MFNetworkBuilder();

  MFBuilderFunctionNode &add_function(const MultiFunction &function,
                                      ArrayRef<uint> input_param_indices,
                                      ArrayRef<uint> output_param_indices);
  MFBuilderPlaceholderNode &add_placeholder(ArrayRef<MFDataType> input_types,
                                            ArrayRef<MFDataType> output_types);
  void add_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to);

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

  ArrayRef<MFBuilderPlaceholderNode *> placeholder_nodes() const
  {
    return m_placeholder_nodes;
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

/* Network
 ******************************************/

class MFNode;
class MFFunctionNode;
class MFPlaceholderNode;

class MFSocket;
class MFInputSocket;
class MFOutputSocket;

class MFNetwork;

class MFNode : BLI::NonCopyable, BLI::NonMovable {
 private:
  const MFNetwork *m_network;
  Vector<const MFInputSocket *> m_inputs;
  Vector<const MFOutputSocket *> m_outputs;
  bool m_is_placeholder;
  uint m_id;

  friend MFNetwork;

 public:
  const MFNetwork &network() const;

  ArrayRef<const MFInputSocket *> inputs() const;
  ArrayRef<const MFOutputSocket *> outputs() const;

  uint id() const;

  bool is_function() const;
  bool is_placeholder() const;

  const MFFunctionNode &as_function() const;
  const MFPlaceholderNode &as_placeholder() const;
};

class MFFunctionNode : public MFNode {
 private:
  const MultiFunction *m_function;
  Vector<uint> m_input_param_indices;
  Vector<uint> m_output_param_indices;

  friend MFNetwork;

 public:
  const MultiFunction &function() const;

  ArrayRef<uint> input_param_indices() const;
  ArrayRef<uint> output_param_indices() const;
};

class MFPlaceholderNode : public MFNode {
};

class MFSocket : BLI::NonCopyable, BLI::NonMovable {
 private:
  MFNode *m_node;
  bool m_is_output;
  uint m_index;
  MFDataType m_type;
  uint m_id;

  friend MFNetwork;

 public:
  const MFNode &node() const;
  MFDataType type() const;

  uint index() const;
  uint id() const;

  bool is_input() const;
  bool is_output() const;

  MFInputSocket &as_input();
  MFOutputSocket &as_output();

  const MFInputSocket &as_input() const;
  const MFOutputSocket &as_output() const;
};

class MFInputSocket : public MFSocket {
 private:
  MFOutputSocket *m_origin;

  friend MFNetwork;

 public:
  const MFOutputSocket &origin() const;
};

class MFOutputSocket : public MFSocket {
 private:
  Vector<const MFInputSocket *> m_targets;

  friend MFNetwork;

 public:
  ArrayRef<const MFInputSocket *> targets() const;
};

class MFNetwork : BLI::NonCopyable, BLI::NonMovable {
 private:
  Array<MFNode *> m_node_by_id;
  Array<MFSocket *> m_socket_by_id;

  Vector<MFFunctionNode *> m_function_nodes;
  Vector<MFPlaceholderNode *> m_placeholder_nodes;
  Vector<MFInputSocket *> m_input_sockets;
  Vector<MFOutputSocket *> m_output_sockets;

 public:
  MFNetwork(std::unique_ptr<MFNetworkBuilder> builder);
  ~MFNetwork();

  const MFNode &node_by_id(uint id) const;
  const MFSocket &socket_by_id(uint id) const;
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

inline uint MFBuilderNode::id()
{
  return m_id;
}

inline bool MFBuilderNode::is_function()
{
  return !m_is_placeholder;
}
inline bool MFBuilderNode::is_placeholder()
{
  return m_is_placeholder;
}

inline MFBuilderFunctionNode &MFBuilderNode::as_function()
{
  BLI_assert(this->is_function());
  return *(MFBuilderFunctionNode *)this;
}

inline MFBuilderPlaceholderNode &MFBuilderNode::as_placeholder()
{
  BLI_assert(this->is_placeholder());
  return *(MFBuilderPlaceholderNode *)this;
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

inline MFDataType MFBuilderSocket::type()
{
  return m_type;
}

inline uint MFBuilderSocket::index()
{
  return m_index;
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

inline uint MFNode::id() const
{
  return m_id;
}

inline bool MFNode::is_function() const
{
  return !m_is_placeholder;
}

inline bool MFNode::is_placeholder() const
{
  return m_is_placeholder;
}

inline const MFFunctionNode &MFNode::as_function() const
{
  BLI_assert(this->is_function());
  return *(MFFunctionNode *)this;
}

inline const MFPlaceholderNode &MFNode::as_placeholder() const
{
  BLI_assert(this->is_placeholder());
  return *(const MFPlaceholderNode *)this;
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

inline const MFNode &MFSocket::node() const
{
  return *m_node;
}

inline MFDataType MFSocket::type() const
{
  return m_type;
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

}  // namespace BKE

#endif /* __BKE_MULTI_FUNCTION_NETWORK_H__ */
