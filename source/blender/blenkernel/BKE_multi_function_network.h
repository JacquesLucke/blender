#ifndef __BKE_MULTI_FUNCTION_NETWORK_H__
#define __BKE_MULTI_FUNCTION_NETWORK_H__

#include "BKE_multi_function.h"

#include "BLI_optional.h"

namespace BKE {

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

class BuilderNode {
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
};

class BuilderPlaceholderNode : public BuilderNode {
};

class BuilderSocket {
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

class NetworkBuilder {
 private:
  Vector<std::unique_ptr<BuilderFunctionNode>> m_function_nodes;
  Vector<std::unique_ptr<BuilderPlaceholderNode>> m_placeholder_nodes;
  Vector<std::unique_ptr<BuilderInputSocket>> m_input_sockets;
  Vector<std::unique_ptr<BuilderOutputSocket>> m_output_sockets;

 public:
  BuilderFunctionNode &add_function(MultiFunction &function,
                                    ArrayRef<uint> input_param_indices,
                                    ArrayRef<uint> output_param_indices);
  BuilderPlaceholderNode &add_placeholder(ArrayRef<MultiFunctionDataType> input_types,
                                          ArrayRef<MultiFunctionDataType> output_types);
  void add_link(BuilderOutputSocket &from, BuilderInputSocket &to);
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

class Node {
 private:
  Network *m_network;
  ArrayRef<InputSocket *> m_inputs;
  ArrayRef<OutputSocket *> m_outputs;
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
  ArrayRef<uint> m_input_param_indices;
  ArrayRef<uint> m_output_param_indices;

  friend Network;

 public:
  MultiFunction &function();
};

class PlaceholderNode : public Node {
};

class Socket {
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
  ArrayRef<InputSocket *> m_targets;

  friend Network;

 public:
  ArrayRef<InputSocket *> targets();
};

class Network {
 private:
  BLI::MonotonicAllocator<> m_allocator;
  Vector<Node *> m_nodes;

 public:
  Network(std::unique_ptr<NetworkBuilder> builder);
};

/* Builder Implementations
 *******************************************/

NetworkBuilder &BuilderNode::network()
{
  return *m_network;
}

ArrayRef<BuilderInputSocket *> BuilderNode::inputs()
{
  return m_inputs;
}
ArrayRef<BuilderOutputSocket *> BuilderNode::outputs()
{
  return m_outputs;
}

uint BuilderNode::id()
{
  return m_id;
}

bool BuilderNode::is_function()
{
  return !m_is_placeholder;
}
bool BuilderNode::is_placeholder()
{
  return m_is_placeholder;
}

BuilderFunctionNode &BuilderNode::as_function()
{
  BLI_assert(this->is_function());
  return *(BuilderFunctionNode *)this;
}
BuilderPlaceholderNode &BuilderNode::as_placeholder()
{
  BLI_assert(this->is_placeholder());
  return *(BuilderPlaceholderNode *)this;
}

MultiFunction &BuilderFunctionNode::function()
{
  return *m_function;
}

BuilderNode &BuilderSocket::node()
{
  return *m_node;
}

MultiFunctionDataType BuilderSocket::type()
{
  return m_type;
}

uint BuilderSocket::id()
{
  return m_id;
}

bool BuilderSocket::is_input()
{
  return !m_is_output;
}
bool BuilderSocket::is_output()
{
  return m_is_output;
}

BuilderInputSocket &BuilderSocket::as_input()
{
  BLI_assert(this->is_input());
  return *(BuilderInputSocket *)this;
}
BuilderOutputSocket &BuilderSocket::as_output()
{
  BLI_assert(this->is_output());
  return *(BuilderOutputSocket *)this;
}

BuilderOutputSocket *BuilderInputSocket::origin()
{
  return m_origin;
}

ArrayRef<BuilderInputSocket *> BuilderOutputSocket::targets()
{
  return m_targets;
}

/* Network Implementations
 **************************************/

Network &Node::network()
{
  return *m_network;
}

ArrayRef<InputSocket *> Node::inputs()
{
  return m_inputs;
}
ArrayRef<OutputSocket *> Node::outputs()
{
  return m_outputs;
}

uint Node::id()
{
  return m_id;
}

bool Node::is_function()
{
  return !m_is_placeholder;
}
bool Node::is_placeholder()
{
  return m_is_placeholder;
}

FunctionNode &Node::as_function()
{
  BLI_assert(this->is_function());
  return *(FunctionNode *)this;
}
PlaceholderNode &Node::as_placeholder()
{
  BLI_assert(this->is_placeholder());
  return *(PlaceholderNode *)this;
}

MultiFunction &FunctionNode::function()
{
  return *m_function;
}

Node &Socket::node()
{
  return *m_node;
}

MultiFunctionDataType Socket::type()
{
  return m_type;
}

uint Socket::id()
{
  return m_id;
}

bool Socket::is_input()
{
  return !m_is_output;
}
bool Socket::is_output()
{
  return m_is_output;
}

InputSocket &Socket::as_input()
{
  BLI_assert(this->is_input());
  return *(InputSocket *)this;
}
OutputSocket &Socket::as_output()
{
  BLI_assert(this->is_output());
  return *(OutputSocket *)this;
}

OutputSocket &InputSocket::origin()
{
  return *m_origin;
}

ArrayRef<InputSocket *> OutputSocket::targets()
{
  return m_targets;
}

}  // namespace MultiFunctionNetwork

}  // namespace BKE

#endif /* __BKE_MULTI_FUNCTION_NETWORK_H__ */
