/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_context_stack.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_set.hh"

namespace blender {

namespace context_stack_map_detail {
template<typename T> struct Item {
  Item *parent = nullptr;
  StringRefNull type;
  Span<std::byte> encoded;
  T *value = nullptr;

  Item(Item *parent, StringRefNull type, Span<std::byte> encoded)
      : parent(parent), type(type), encoded(encoded)
  {
  }

  uint64_t hash() const
  {
    return get_default_hash_3(parent, type, encoded);
  }

  friend bool operator==(const Item &a, const Item &b)
  {
    return a.parent == b.parent && a.type == b.type && a.encoded == b.encoded;
  }
};
}  // namespace context_stack_map_detail

template<typename T> class ContextStackMap {
 private:
  using Item = context_stack_map_detail::Item<T>;

  LinearAllocator<> &allocator_;
  Set<std::reference_wrapper<Item>> set_;

 public:
  T &lookup(const ContextStack &context_stack)
  {
    const ContextStack *parent = context_stack.parent();
    if (parent == nullptr) {
    }
  }

 private:
  Item &lookup_item(const ContextStack &context_stack)
  {
    const ContextStack *parent = context_stack.parent();
    const Span<std::byte> encoded = context_stack.encoded();
    const StringRefNull type = context_stack.type();
    if (parent == nullptr) {
      return this->lookup_or_add_item(nullptr, encoded, type);
    }
    Item &parent_item = this->lookup_item(*parent);
    Item &item = set_.lookup_key_or_add(Item{
        &parent_item,
    })
  }

  Item &lookup_or_add_item(const Item *parent,
                           const Span<std::byte> encoded,
                           const StringRefNull type)
  {
    Item local_item{parent, type, encoded};
    std::reference_wrapper<Item> &stored_item = set_.lookup_key_or_add(local_item);
    if (&local_item == &stored_item.get()) {
      const Span<std::byte> new_stored_encoding = allocator_.construct_array_copy(encoded);
      Item &new_stored_item = allocator_.construct<Item>(parent, type, new_stored_encoding);
      stored_item = std::ref(new_stored_item);
    }
    return item;
  }
};

}  // namespace blender
