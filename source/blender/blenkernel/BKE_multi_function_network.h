#ifndef __BKE_MULTI_FUNCTION_NETWORK_H__
#define __BKE_MULTI_FUNCTION_NETWORK_H__

#include "BKE_multi_function.h"

#include "BLI_optional.h"
#include "BLI_array_cxx.h"

namespace BKE {

using BLI::Array;
using BLI::Optional;

namespace MultiFunctionNetwork {

/* Network Builder
 ****************************************/

class BuilderNode;
class BuilderFunctionNode;
class BuilderPlaceholderNode;

class BuilderSocket;
class BuilderInputSocket;
class BuilderOutputSocket;

class NetworkBuilder;

class BuilderNode : BLI::NonCopyable, BLI::NonMovable {
 protected:
  NetworkBuilder *m_network;
  Vector<BuilderInputSocket *> m_inputs;
  Vector<BuilderOutputSocket *> m_outputs;
  uint m_id;
  bool m_is_placeholder;

  friend NetworkBuilder;

 public:
  NetworkBuilder &network();

  ArrayRef<BuilderInputSocket *> inputs();
  ArrayRef<BuilderOutputSocket *> outputs();

  uint id();

  bool is_function();
  bool is_placeholder();

  BuilderFunctionNode &as_function();
  BuilderPlaceholderNode &as_placeholder();
};

class BuilderFunctionNode : public BuilderNode {
 private:
  MultiFunction *m_function;
  Vector<uint> m_input_param_indices;
  Vector<uint> m_output_param_indices;

  friend NetworkBuilder;

 public:
  MultiFunction &function();

  ArrayRef<uint> input_param_indices();
  ArrayRef<uint> output_param_indices();
};

class BuilderPlaceholderNode : public BuilderNode {
};

class BuilderSocket : BLI::NonCopyable, BLI::NonMovable {
 private:
  BuilderNode *m_node;
  bool m_is_output;
  uint m_index;
  MultiFunctionDataType m_type;
  uint m_id;

  friend NetworkBuilder;

 public:
  BuilderNode &node();
  MultiFunctionDataType type();

  uint index();
  uint id();

  bool is_input();
  bool is_output();

  BuilderInputSocket &as_input();
  BuilderOutputSocket &as_output();
};

class BuilderInputSocket : public BuilderSocket {
 private:
  BuilderOutputSocket *m_origin;

  friend NetworkBuilder;

 public:
  BuilderOutputSocket *origin();
};

class BuilderOutputSocket : public BuilderSocket {
 private:
  Vector<BuilderInputSocket *> m_targets;

  friend NetworkBuilder;

 public:
  ArrayRef<BuilderInputSocket *> targets();
};

class NetworkBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  Vector<BuilderNode *> m_node_by_id;
  Vector<BuilderSocket *> m_socket_by_id;

  Vector<BuilderFunctionNode *> m_function_nodes;
  Vector<BuilderPlaceholderNode *> m_placeholder_nodes;
  Vector<BuilderInputSocket *> m_input_sockets;
  Vector<BuilderOutputSocket *> m_output_sockets;

 public:
  ~NetworkBuilder();

  BuilderFunctionNode &add_function(MultiFunction &function,
                                    ArrayRef<uint> input_param_indices,
                                    ArrayRef<uint> output_param_indices);
  BuilderPlaceholderNode &add_placeholder(ArrayRef<MultiFunctionDataType> input_types,
                                          ArrayRef<MultiFunctionDataType> output_types);
  void add_link(BuilderOutputSocket &from, BuilderInputSocket &to);

  ArrayRef<BuilderNode *> nodes_by_id() const
  {
    return m_node_by_id;
  }

  ArrayRef<BuilderSocket *> sockets_by_id() const
  {
    return m_socket_by_id;
  }

  ArrayRef<BuilderFunctionNode *> function_nodes() const
  {
    return m_function_nodes;
  }

  ArrayRef<BuilderPlaceholderNode *> placeholder_nodes() const
  {
    return m_placeholder_nodes;
  }

  ArrayRef<BuilderInputSocket *> input_sockets() const
  {
    return m_input_sockets;
  }

  ArrayRef<BuilderOutputSocket *> output_sockets() const
  {
    return m_output_sockets;
  }
};

/* Network
 ******************************************/

class Node;
class FunctionNode;
class PlaceholderNode;

class Socket;
class InputSocket;
class OutputSocket;

class Network;

class Node : BLI::NonCopyable, BLI::NonMovable {
 private:
  Network *m_network;
  Vector<InputSocket *> m_inputs;
  Vector<OutputSocket *> m_outputs;
  bool m_is_placeholder;
  uint m_id;

  friend Network;

 public:
  Network &network();

  ArrayRef<InputSocket *> inputs();
  ArrayRef<OutputSocket *> outputs();

  uint id();

  bool is_function();
  bool is_placeholder();

  FunctionNode &as_function();
  PlaceholderNode &as_placeholder();
};

class FunctionNode : public Node {
 private:
  MultiFunction *m_function;
  Vector<uint> m_input_param_indices;
  Vector<uint> m_output_param_indices;

  friend Network;

 public:
  MultiFunction &function();
};

class PlaceholderNode : public Node {
};

class Socket : BLI::NonCopyable, BLI::NonMovable {
 private:
  Node *m_node;
  bool m_is_output;
  uint m_index;
  MultiFunctionDataType m_type;
  uint m_id;

  friend Network;

 public:
  Node &node();
  MultiFunctionDataType type();

  uint index();
  uint id();

  bool is_input();
  bool is_output();

  InputSocket &as_input();
  OutputSocket &as_output();
};

class InputSocket : public Socket {
 private:
  OutputSocket *m_origin;

  friend Network;

 public:
  OutputSocket &origin();
};

class OutputSocket : public Socket {
 private:
  Vector<InputSocket *> m_targets;

  friend Network;

 public:
  ArrayRef<InputSocket *> targets();
};

class Network : BLI::NonCopyable, BLI::NonMovable {
 private:
  Array<Node *> m_node_by_id;
  Array<Socket *> m_socket_by_id;

  Vector<FunctionNode *> m_function_nodes;
  Vector<PlaceholderNode *> m_placeholder_nodes;
  Vector<InputSocket *> m_input_sockets;
  Vector<OutputSocket *> m_output_sockets;

 public:
  Network(std::unique_ptr<NetworkBuilder> builder);
  ~Network();

  Node &node_by_id(uint id);
};

/* Builder Implementations
 *******************************************/

inline NetworkBuilder &BuilderNode::network()
{
  return *m_network;
}

inline ArrayRef<BuilderInputSocket *> BuilderNode::inputs()
{
  return m_inputs;
}
inline ArrayRef<BuilderOutputSocket *> BuilderNode::outputs()
{
  return m_outputs;
}

inline uint BuilderNode::id()
{
  return m_id;
}

inline bool BuilderNode::is_function()
{
  return !m_is_placeholder;
}
inline bool BuilderNode::is_placeholder()
{
  return m_is_placeholder;
}

inline BuilderFunctionNode &BuilderNode::as_function()
{
  BLI_assert(this->is_function());
  return *(BuilderFunctionNode *)this;
}

inline BuilderPlaceholderNode &BuilderNode::as_placeholder()
{
  BLI_assert(this->is_placeholder());
  return *(BuilderPlaceholderNode *)this;
}

inline MultiFunction &BuilderFunctionNode::function()
{
  return *m_function;
}

inline ArrayRef<uint> BuilderFunctionNode::input_param_indices()
{
  return m_input_param_indices;
}

inline ArrayRef<uint> BuilderFunctionNode::output_param_indices()
{
  return m_output_param_indices;
}

inline BuilderNode &BuilderSocket::node()
{
  return *m_node;
}

inline MultiFunctionDataType BuilderSocket::type()
{
  return m_type;
}

inline uint BuilderSocket::index()
{
  return m_index;
}

inline uint BuilderSocket::id()
{
  return m_id;
}

inline bool BuilderSocket::is_input()
{
  return !m_is_output;
}
inline bool BuilderSocket::is_output()
{
  return m_is_output;
}

inline BuilderInputSocket &BuilderSocket::as_input()
{
  BLI_assert(this->is_input());
  return *(BuilderInputSocket *)this;
}
inline BuilderOutputSocket &BuilderSocket::as_output()
{
  BLI_assert(this->is_output());
  return *(BuilderOutputSocket *)this;
}

inline BuilderOutputSocket *BuilderInputSocket::origin()
{
  return m_origin;
}

inline ArrayRef<BuilderInputSocket *> BuilderOutputSocket::targets()
{
  return m_targets;
}

/* Network Implementations
 **************************************/

inline Network &Node::network()
{
  return *m_network;
}

inline ArrayRef<InputSocket *> Node::inputs()
{
  return m_inputs;
}

inline ArrayRef<OutputSocket *> Node::outputs()
{
  return m_outputs;
}

inline uint Node::id()
{
  return m_id;
}

inline bool Node::is_function()
{
  return !m_is_placeholder;
}

inline bool Node::is_placeholder()
{
  return m_is_placeholder;
}

inline FunctionNode &Node::as_function()
{
  BLI_assert(this->is_function());
  return *(FunctionNode *)this;
}

inline PlaceholderNode &Node::as_placeholder()
{
  BLI_assert(this->is_placeholder());
  return *(PlaceholderNode *)this;
}

inline MultiFunction &FunctionNode::function()
{
  return *m_function;
}

inline Node &Socket::node()
{
  return *m_node;
}

inline MultiFunctionDataType Socket::type()
{
  return m_type;
}

inline uint Socket::index()
{
  return m_index;
}

inline uint Socket::id()
{
  return m_id;
}

inline bool Socket::is_input()
{
  return !m_is_output;
}

inline bool Socket::is_output()
{
  return m_is_output;
}

inline InputSocket &Socket::as_input()
{
  BLI_assert(this->is_input());
  return *(InputSocket *)this;
}

inline OutputSocket &Socket::as_output()
{
  BLI_assert(this->is_output());
  return *(OutputSocket *)this;
}

inline OutputSocket &InputSocket::origin()
{
  return *m_origin;
}

inline ArrayRef<InputSocket *> OutputSocket::targets()
{
  return m_targets;
}

inline Node &Network::node_by_id(uint index)
{
  return *m_node_by_id[index];
}

}  // namespace MultiFunctionNetwork

}  // namespace BKE

#endif /* __BKE_MULTI_FUNCTION_NETWORK_H__ */
