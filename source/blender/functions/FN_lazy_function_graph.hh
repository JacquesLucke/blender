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
  LFNode *node_;
  const CPPType *type_;
  bool is_input_;
  int index_in_node_;

  friend LazyFunctionGraph;

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

  const CPPType &type() const;

  std::string name() const;
};

class LFInputSocket : public LFSocket {
 private:
  LFOutputSocket *origin_;
  const void *default_value_ = nullptr;

  friend LazyFunctionGraph;

 public:
  LFOutputSocket *origin();
  const LFOutputSocket *origin() const;

  const void *default_value() const;
  void set_default_value(const void *value);
};

class LFOutputSocket : public LFSocket {
 private:
  Vector<LFInputSocket *> targets_;

  friend LazyFunctionGraph;

 public:
  Span<LFInputSocket *> targets();
  Span<const LFInputSocket *> targets() const;
};

class LFNode : NonCopyable, NonMovable {
 private:
  const LazyFunction *fn_ = nullptr;
  Span<LFInputSocket *> inputs_;
  Span<LFOutputSocket *> outputs_;

  friend LazyFunctionGraph;

 public:
  const LazyFunction &function() const;

  Span<const LFInputSocket *> inputs() const;
  Span<const LFOutputSocket *> outputs() const;
  Span<LFInputSocket *> inputs();
  Span<LFOutputSocket *> outputs();

  std::string name() const;
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

  std::string to_dot() const;
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

inline const CPPType &LFSocket::type() const
{
  return *type_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #LFInputSocket Inline Methods
 * \{ */

inline const LFOutputSocket *LFInputSocket::origin() const
{
  return origin_;
}

inline LFOutputSocket *LFInputSocket::origin()
{
  return origin_;
}

inline const void *LFInputSocket::default_value() const
{
  return default_value_;
}

inline void LFInputSocket::set_default_value(const void *value)
{
  default_value_ = value;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #LFOutputSocket Inline Methods
 * \{ */

inline Span<const LFInputSocket *> LFOutputSocket::targets() const
{
  return targets_;
}

inline Span<LFInputSocket *> LFOutputSocket::targets()
{
  return targets_;
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
