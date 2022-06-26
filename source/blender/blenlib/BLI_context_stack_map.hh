/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_context_stack.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"

namespace blender {

template<typename T> class ContextStackMap {
 private:
  LinearAllocator<> allocator_;
  Map<ContextStackHash, destruct_ptr<T>> map_;

 public:
  T &lookup_or_add(const ContextStack &context_stack)
  {
    const ContextStackHash &hash = context_stack.hash();
    destruct_ptr<T> &value = map_.lookup_or_add_cb(hash,
                                                   [&]() { return allocator_.construct<T>(); });
    return *value;
  }

  const T *lookup_ptr(const ContextStack &context_stack) const
  {
    const ContextStackHash &hash = context_stack.hash();
    const destruct_ptr<T> *value = map_.lookup_ptr(hash);
    if (value != nullptr) {
      return value->get();
    }
    return nullptr;
  }

  const T &lookup_or_default(const ContextStack &context_stack, const T &default_value) const
  {
    const T *value = this->lookup_ptr(context_stack);
    if (value != nullptr) {
      return *value;
    }
    return default_value;
  }

  T *lookup_ptr(const ContextStack &context_stack)
  {
    return const_cast<T *>(const_cast<const ContextStackMap *>(this)->lookup_ptr(context_stack));
  }
};

}  // namespace blender
