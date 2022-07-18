/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "BLI_linear_allocator.hh"

#include "FN_lazy_function.hh"

namespace blender::fn::lazy_function {

class Socket;
class InputSocket;
class OutputSocket;
class Node;
class LazyFunctionGraph;

class Socket : NonCopyable, NonMovable {
 protected:
  Node *node_;
  const CPPType *type_;
  bool is_input_;
  int index_in_node_;

  friend LazyFunctionGraph;

 public:
  bool is_input() const;
  bool is_output() const;

  int index_in_node() const;

  InputSocket &as_input();
  OutputSocket &as_output();
  const InputSocket &as_input() const;
  const OutputSocket &as_output() const;

  const Node &node() const;
  Node &node();

  const CPPType &type() const;

  std::string name() const;
};

class InputSocket : public Socket {
 private:
  OutputSocket *origin_;
  const void *default_value_ = nullptr;

  friend LazyFunctionGraph;

 public:
  OutputSocket *origin();
  const OutputSocket *origin() const;

  const void *default_value() const;
  void set_default_value(const void *value);
};

class OutputSocket : public Socket {
 private:
  Vector<InputSocket *> targets_;

  friend LazyFunctionGraph;

 public:
  Span<InputSocket *> targets();
  Span<const InputSocket *> targets() const;
};

class Node : NonCopyable, NonMovable {
 protected:
  const LazyFunction *fn_ = nullptr;
  Span<InputSocket *> inputs_;
  Span<OutputSocket *> outputs_;
  int index_in_graph_ = -1;

  friend LazyFunctionGraph;

 public:
  bool is_dummy() const;
  bool is_function() const;
  int index_in_graph() const;

  Span<const InputSocket *> inputs() const;
  Span<const OutputSocket *> outputs() const;
  Span<InputSocket *> inputs();
  Span<OutputSocket *> outputs();

  const InputSocket &input(int index) const;
  const OutputSocket &output(int index) const;
  InputSocket &input(int index);
  OutputSocket &output(int index);

  std::string name() const;
};

class FunctionNode : public Node {
 public:
  const LazyFunction &function() const;
};

class DummyNode : public Node {
 private:
  std::string name_;

  friend Node;
};

class LazyFunctionGraph : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  Vector<Node *> nodes_;

 public:
  ~LazyFunctionGraph();

  Span<const Node *> nodes() const;

  FunctionNode &add_function(const LazyFunction &fn);
  DummyNode &add_dummy(Span<const CPPType *> input_types, Span<const CPPType *> output_types);
  void add_link(OutputSocket &from, InputSocket &to);
  void remove_link(OutputSocket &from, InputSocket &to);

  void update_node_indices();
  bool node_indices_are_valid() const;

  std::string to_dot() const;
};

/* -------------------------------------------------------------------- */
/** \name #Socket Inline Methods
 * \{ */

inline bool Socket::is_input() const
{
  return is_input_;
}

inline bool Socket::is_output() const
{
  return !is_input_;
}

inline int Socket::index_in_node() const
{
  return index_in_node_;
}

inline InputSocket &Socket::as_input()
{
  BLI_assert(this->is_input());
  return *static_cast<InputSocket *>(this);
}

inline OutputSocket &Socket::as_output()
{
  BLI_assert(this->is_output());
  return *static_cast<OutputSocket *>(this);
}

inline const InputSocket &Socket::as_input() const
{
  BLI_assert(this->is_input());
  return *static_cast<const InputSocket *>(this);
}

inline const OutputSocket &Socket::as_output() const
{
  BLI_assert(this->is_output());
  return *static_cast<const OutputSocket *>(this);
}

inline const Node &Socket::node() const
{
  return *node_;
}

inline Node &Socket::node()
{
  return *node_;
}

inline const CPPType &Socket::type() const
{
  return *type_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #InputSocket Inline Methods
 * \{ */

inline const OutputSocket *InputSocket::origin() const
{
  return origin_;
}

inline OutputSocket *InputSocket::origin()
{
  return origin_;
}

inline const void *InputSocket::default_value() const
{
  return default_value_;
}

inline void InputSocket::set_default_value(const void *value)
{
  default_value_ = value;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #OutputSocket Inline Methods
 * \{ */

inline Span<const InputSocket *> OutputSocket::targets() const
{
  return targets_;
}

inline Span<InputSocket *> OutputSocket::targets()
{
  return targets_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Node Inline Methods
 * \{ */

inline bool Node::is_dummy() const
{
  return fn_ == nullptr;
}

inline bool Node::is_function() const
{
  return fn_ != nullptr;
}

inline int Node::index_in_graph() const
{
  return index_in_graph_;
}

inline Span<const InputSocket *> Node::inputs() const
{
  return inputs_;
}

inline Span<const OutputSocket *> Node::outputs() const
{
  return outputs_;
}

inline Span<InputSocket *> Node::inputs()
{
  return inputs_;
}

inline Span<OutputSocket *> Node::outputs()
{
  return outputs_;
}

inline const InputSocket &Node::input(const int index) const
{
  return *inputs_[index];
}

inline const OutputSocket &Node::output(const int index) const
{
  return *outputs_[index];
}

inline InputSocket &Node::input(const int index)
{
  return *inputs_[index];
}

inline OutputSocket &Node::output(const int index)
{
  return *outputs_[index];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #FunctionNode Inline Methods
 * \{ */

inline const LazyFunction &FunctionNode::function() const
{
  BLI_assert(fn_ != nullptr);
  return *fn_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #LazyFunctionGraph Inline Methods
 * \{ */

inline Span<const Node *> LazyFunctionGraph::nodes() const
{
  return nodes_;
}

/** \} */

}  // namespace blender::fn::lazy_function
