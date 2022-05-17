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
 private:
  Vector<LFSocket *> linked_sockets_;
  int index_in_node_;
  bool is_input_;
  LFNode *node_;

 public:
  bool is_input() const
  {
    return is_input_;
  }

  bool is_output() const
  {
    return !is_input_;
  }

  int index_in_node() const
  {
    return index_in_node_;
  }

  LFInputSocket &as_input()
  {
    BLI_assert(this->is_input());
    return *static_cast<LFSocket *>(this);
  }

  LFOutputSocket &as_output()
  {
    BLI_assert(this->is_output());
    return *static_cast<LFSocket *>(this);
  }

  const LFInputSocket &as_input() const
  {
    BLI_assert(this->is_input());
    return *static_cast<const LFSocket *>(this);
  }

  const LFOutputSocket &as_output() const
  {
    BLI_assert(this->is_output());
    return *static_cast<const LFSocket *>(this);
  }

  const LFNode &node() const
  {
    return *node_;
  }

  LFNode &node()
  {
    return *node_;
  }

  Span<const LFSocket *> links() const
  {
    return linked_sockets_.as_span();
  }

  Span<LFSocket *> links()
  {
    return linked_sockets_.as_span();
  }
};

class LFInputSocket : public LFSocket {
 public:
  Span<const LFOutputSocket *> links() const
  {
    return linked_sockets_.as_span().cast<const LFOutputSocket *>();
  }

  Span<LFOutputSocket *> links()
  {
    return linked_sockets.as_span().cast<LFOutputSocket *>();
  }
};

class LFOutputSocket : public LFSocket {
 public:
  Span<const LFInputSocket *> links() const
  {
    return linked_sockets_.as_span().cast<const LFInputSocket *>();
  }

  Span<LFInputSocket *> links()
  {
    return linked_sockets.as_span().cast<LFInputSocket *>();
  }
};

class LFNode : NonCopyable, NonMovable {
 private:
  const LazyFunction *fn_ = nullptr;
  Vector<LFInputSocket *> inputs_;
  Vector<LFOutputSocket *> outputs_;

 public:
  const LazyFunction &function() const
  {
    return *fn_;
  }

  Span<const LFInputSocket *> inputs() const
  {
    return inputs_;
  }

  Span<const LFOutputSocket *> outputs() const
  {
    return outputs_;
  }

  Span<LFInputSocket *> inputs()
  {
    return inputs_;
  }

  Span<LFOutputSocket *> outputs()
  {
    return outputs_;
  }
};

class LazyFunctionGraph : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  Vector<LFNode *> nodes_;

 public:
  ~LazyFunctionGraph();

  LFNode &add_node(const LazyFunction &fn);
  void add_link(LFOutputSocket &from, LFInputSocket &to);
};

}  // namespace blender::fn
