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
  MultiFunction *m_function;
  Vector<uint> m_input_param_indices;
  Vector<uint> m_output_param_indices;

  friend MFNetworkBuilder;

 public:
  MultiFunction &function();

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

  MFBuilderFunctionNode &add_function(MultiFunction &function,
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
  MFNetwork *m_network;
  Vector<MFInputSocket *> m_inputs;
  Vector<MFOutputSocket *> m_outputs;
  bool m_is_placeholder;
  uint m_id;

  friend MFNetwork;

 public:
  MFNetwork &network();

  ArrayRef<MFInputSocket *> inputs();
  ArrayRef<MFOutputSocket *> outputs();

  uint id();

  bool is_function();
  bool is_placeholder();

  MFFunctionNode &as_function();
  MFPlaceholderNode &as_placeholder();
};

class MFFunctionNode : public MFNode {
 private:
  MultiFunction *m_function;
  Vector<uint> m_input_param_indices;
  Vector<uint> m_output_param_indices;

  friend MFNetwork;

 public:
  MultiFunction &function();
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
  MFNode &node();
  MFDataType type();

  uint index();
  uint id();

  bool is_input();
  bool is_output();

  MFInputSocket &as_input();
  MFOutputSocket &as_output();
};

class MFInputSocket : public MFSocket {
 private:
  MFOutputSocket *m_origin;

  friend MFNetwork;

 public:
  MFOutputSocket &origin();
};

class MFOutputSocket : public MFSocket {
 private:
  Vector<MFInputSocket *> m_targets;

  friend MFNetwork;

 public:
  ArrayRef<MFInputSocket *> targets();
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

  MFNode &node_by_id(uint id);
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

inline MultiFunction &MFBuilderFunctionNode::function()
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

inline MFNetwork &MFNode::network()
{
  return *m_network;
}

inline ArrayRef<MFInputSocket *> MFNode::inputs()
{
  return m_inputs;
}

inline ArrayRef<MFOutputSocket *> MFNode::outputs()
{
  return m_outputs;
}

inline uint MFNode::id()
{
  return m_id;
}

inline bool MFNode::is_function()
{
  return !m_is_placeholder;
}

inline bool MFNode::is_placeholder()
{
  return m_is_placeholder;
}

inline MFFunctionNode &MFNode::as_function()
{
  BLI_assert(this->is_function());
  return *(MFFunctionNode *)this;
}

inline MFPlaceholderNode &MFNode::as_placeholder()
{
  BLI_assert(this->is_placeholder());
  return *(MFPlaceholderNode *)this;
}

inline MultiFunction &MFFunctionNode::function()
{
  return *m_function;
}

inline MFNode &MFSocket::node()
{
  return *m_node;
}

inline MFDataType MFSocket::type()
{
  return m_type;
}

inline uint MFSocket::index()
{
  return m_index;
}

inline uint MFSocket::id()
{
  return m_id;
}

inline bool MFSocket::is_input()
{
  return !m_is_output;
}

inline bool MFSocket::is_output()
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

inline MFOutputSocket &MFInputSocket::origin()
{
  return *m_origin;
}

inline ArrayRef<MFInputSocket *> MFOutputSocket::targets()
{
  return m_targets;
}

inline MFNode &MFNetwork::node_by_id(uint index)
{
  return *m_node_by_id[index];
}

}  // namespace BKE

#endif /* __BKE_MULTI_FUNCTION_NETWORK_H__ */
