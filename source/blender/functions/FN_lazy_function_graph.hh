/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "BLI_linear_allocator.hh"

#include "FN_lazy_function.hh"

namespace blender::fn {

class LFSocket;
class LFInputSocket;
class LFOutputSocket;
class LFNode;
class LazyFunctionGraph;

class LFSocket : NonCopyable, NonMovable {
 protected:
  Vector<LFSocket *> linked_sockets_;
  int index_in_node_;
  bool is_input_;
  LFNode *node_;

 public:
  bool is_input() const;
  bool is_output() const;

  int index_in_node() const;

  LFInputSocket &as_input();
  LFOutputSocket &as_output();
  const LFInputSocket &as_input() const;
  const LFOutputSocket &as_output() const;

  const LFNode &node() const;
  LFNode &node();

  Span<LFSocket *> links();
  Span<const LFSocket *> links() const;
};

class LFInputSocket : public LFSocket {
 public:
  Span<LFOutputSocket *> links();
  Span<const LFOutputSocket *> links() const;
};

class LFOutputSocket : public LFSocket {
 public:
  Span<LFInputSocket *> links();
  Span<const LFInputSocket *> links() const;
};

class LFNode : NonCopyable, NonMovable {
 private:
  const LazyFunction *fn_ = nullptr;
  Vector<LFInputSocket *> inputs_;
  Vector<LFOutputSocket *> outputs_;

 public:
  const LazyFunction &function() const;

  Span<const LFInputSocket *> inputs() const;
  Span<const LFOutputSocket *> outputs() const;
  Span<LFInputSocket *> inputs();
  Span<LFOutputSocket *> outputs();
};

class LazyFunctionGraph : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  Vector<LFNode *> nodes_;

 public:
  ~LazyFunctionGraph();

  Span<const LFNode *> nodes() const;

  LFNode &add_node(const LazyFunction &fn);
  void add_link(LFOutputSocket &from, LFInputSocket &to);
};

/* -------------------------------------------------------------------- */
/** \name #LFSocket Inline Methods
 * \{ */

inline bool LFSocket::is_input() const
{
  return is_input_;
}

inline bool LFSocket::is_output() const
{
  return !is_input_;
}

inline int LFSocket::index_in_node() const
{
  return index_in_node_;
}

inline LFInputSocket &LFSocket::as_input()
{
  BLI_assert(this->is_input());
  return *static_cast<LFInputSocket *>(this);
}

inline LFOutputSocket &LFSocket::as_output()
{
  BLI_assert(this->is_output());
  return *static_cast<LFOutputSocket *>(this);
}

inline const LFInputSocket &LFSocket::as_input() const
{
  BLI_assert(this->is_input());
  return *static_cast<const LFInputSocket *>(this);
}

inline const LFOutputSocket &LFSocket::as_output() const
{
  BLI_assert(this->is_output());
  return *static_cast<const LFOutputSocket *>(this);
}

inline const LFNode &LFSocket::node() const
{
  return *node_;
}

inline LFNode &LFSocket::node()
{
  return *node_;
}

inline Span<const LFSocket *> LFSocket::links() const
{
  return linked_sockets_.as_span();
}

inline Span<LFSocket *> LFSocket::links()
{
  return linked_sockets_.as_span();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #LFInputSocket Inline Methods
 * \{ */

inline Span<const LFOutputSocket *> LFInputSocket::links() const
{
  return linked_sockets_.as_span().cast<const LFOutputSocket *>();
}

inline Span<LFOutputSocket *> LFInputSocket::links()
{
  return linked_sockets_.as_span().cast<LFOutputSocket *>();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #LFOutputSocket Inline Methods
 * \{ */

inline Span<const LFInputSocket *> LFOutputSocket::links() const
{
  return linked_sockets_.as_span().cast<const LFInputSocket *>();
}

inline Span<LFInputSocket *> LFOutputSocket::links()
{
  return linked_sockets_.as_span().cast<LFInputSocket *>();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #LFNode Inline Methods
 * \{ */

inline const LazyFunction &LFNode::function() const
{
  return *fn_;
}

inline Span<const LFInputSocket *> LFNode::inputs() const
{
  return inputs_;
}

inline Span<const LFOutputSocket *> LFNode::outputs() const
{
  return outputs_;
}

inline Span<LFInputSocket *> LFNode::inputs()
{
  return inputs_;
}

inline Span<LFOutputSocket *> LFNode::outputs()
{
  return outputs_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #LazyFunctionGraph Inline Methods
 * \{ */

inline Span<const LFNode *> LazyFunctionGraph::nodes() const
{
  return nodes_;
}

/** \} */

}  // namespace blender::fn
