/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "BLI_linear_allocator.hh"

#include "FN_lazy_function.hh"

namespace blender::fn {

class LFSocket : NonCopyable, NonMovable {
 private:
  Vector<LFSocket *> linked_sockets_;
  int index_;
  bool is_input_;
};

class LFInputSocket : public LFSocket {
};

class LFOutputSocket : public LFSocket {
};

class LFNode : NonCopyable, NonMovable {
 private:
  const LazyFunction *fn_ = nullptr;
  Vector<LFInputSocket *> inputs_;
  Vector<LFOutputSocket *> outputs_;
};

class LazyFunctionGraph : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  Vector<LFNode *> nodes_;

 public:
  ~LazyFunctionGraph();

  LFNode &add_node(const LazyFunction &fn) const;
};

}  // namespace blender::fn
