/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_context_stack.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"

namespace blender {

namespace context_stack_map_detail {
template<typename T> struct Value {
  T value;
  const char *static_type;
  std::optional<ContextStackHash> parent_hash;
};
}  // namespace context_stack_map_detail

template<typename T> class ContextStackMap {
 private:
  using Value = context_stack_map_detail::Value<T>;

  LinearAllocator<> allocator_;
  Map<ContextStackHash, destruct_ptr<Value>> map_;

 public:
  T &lookup_or_add(const ContextStack &context_stack)
  {
    const ContextStackHash &hash = context_stack.hash();
    destruct_ptr<Value> &value = map_.lookup_or_add_cb(hash, [&]() {
      destruct_ptr<Value> value = allocator_.construct<Value>();
      value->static_type = context_stack.static_type();
      const ContextStack *parent = context_stack.parent();
      if (parent != nullptr) {
        value->parent_hash = parent->hash();
      }
      return value;
    });
    return value->value;
  }

  const T &lookup_or_default(const ContextStack &context_stack, const T &default_value) const
  {
    const ContextStackHash &hash = context_stack.hash();
    const destruct_ptr<Value> *value = map_.lookup_ptr(hash);
    if (value != nullptr) {
      return value->get()->value;
    }
    return default_value;
  }
};

}  // namespace blender
