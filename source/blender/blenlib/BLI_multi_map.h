/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bli
 *
 * A multimap is a map that allows storing multiple values per key.
 */

#pragma once

#include "BLI_map.h"
#include "BLI_array_ref.h"
#include "BLI_vector.h"
#include "BLI_monotonic_allocator.h"

namespace BLI {

template<typename KeyT, typename ValueT, uint N = 4> class MultiMap {
 private:
  struct Entry {
    ValueT *ptr = nullptr;
    uint length = 0;
    uint capacity = 0;
  };

  MonotonicAllocator<sizeof(ValueT) * N> m_allocator;
  Map<KeyT, Entry> m_map;

 public:
  MultiMap() = default;

  ~MultiMap()
  {
    this->foreach_value([](ValueT &value) { value.~ValueT(); });
  }

  MultiMap(const MultiMap &other)
  {
    this->add_multiple(other);
  }

  MultiMap &operator=(const MultiMap &other)
  {
    if (this == &other) {
      return *this;
    }
    this->~MultiMap();
    new (this) MultiMap(other);
    return *this;
  }

  uint key_amount() const
  {
    return m_map.size();
  }

  uint value_amount(const KeyT &key) const
  {
    return m_map.lookup_default(key, {}).length;
  }

  void add_new(const KeyT &key, const ValueT &value)
  {
    BLI_assert(!this->contains(key));
    this->add(key, value);
  }

  void add_multiple_new(const KeyT &key, ArrayRef<ValueT> values)
  {
    BLI_assert(!this->contains(key));
    this->add_multiple(key, values);
  }

  bool add(const KeyT &key, const ValueT &value)
  {
    return this->add__impl(key, value);
  }
  bool add(const KeyT &key, ValueT &&value)
  {
    return this->add__impl(key, std::move(value));
  }
  bool add(KeyT &&key, const ValueT &value)
  {
    return this->add__impl(std::move(key), value);
  }
  bool add(KeyT &&key, ValueT &&value)
  {
    return this->add__impl(std::move(key), std::move(value));
  }

  void add_multiple(const KeyT &key, ArrayRef<ValueT> values)
  {
    this->add_multiple__impl(key, values);
  }
  void add_multiple(const KeyT &&key, ArrayRef<ValueT> values)
  {
    this->add_multiple__impl(std::move(key), values);
  }

  template<uint OtherN> void add_multiple(const MultiMap<KeyT, ValueT, OtherN> &other)
  {
    BLI_assert(this != &other);
    other.foreach_item(
        [&](const KeyT &key, ArrayRef<ValueT> values) { this->add_multiple(key, values); });
  }

  ArrayRef<ValueT> lookup(const KeyT &key) const
  {
    const Entry &entry = m_map.lookup(key);
    return ArrayRef<ValueT>(entry.ptr, entry.length);
  }

  ArrayRef<ValueT> lookup_default(const KeyT &key,
                                  ArrayRef<ValueT> default_array = ArrayRef<ValueT>()) const
  {
    const Entry *entry = m_map.lookup_ptr(key);
    if (entry == nullptr) {
      return default_array;
    }
    else {
      return ArrayRef<ValueT>(entry->ptr, entry->length);
    }
  }

  bool contains(const KeyT &key) const
  {
    return m_map.contains(key);
  }

  typename Map<KeyT, Entry>::KeyIterator keys() const
  {
    return m_map.keys();
  }

  template<typename FuncT> void foreach_value(const FuncT &func) const
  {
    for (const Entry &entry : m_map.values()) {
      for (const ValueT &value : ArrayRef<ValueT>(entry.ptr, entry.length)) {
        func(value);
      }
    }
  }

  template<typename FuncT> void foreach_value(const FuncT &func)
  {
    for (Entry &entry : m_map.values()) {
      for (ValueT &value : MutableArrayRef<ValueT>(entry.ptr, entry.length)) {
        func(value);
      }
    }
  }

  template<typename FuncT> void foreach_item(const FuncT &func) const
  {
    for (auto item : m_map.items()) {
      const KeyT &key = item.key;
      ArrayRef<ValueT> values{item.value.ptr, item.value.length};
      func(key, values);
    }
  }

 private:
  template<typename ForwardKeyT>
  void add_multiple__impl(ForwardKeyT &&key, ArrayRef<ValueT> values)
  {
    for (const ValueT &value : values) {
      this->add(key, value);
    }
  }

  template<typename ForwardKeyT, typename ForwardValueT>
  bool add__impl(ForwardKeyT &&key, ForwardValueT &&value)
  {
    bool newly_inserted = m_map.add_or_modify(
        std::forward<ForwardKeyT>(key),
        /* Insert new key with value. */
        [&](Entry *r_entry) -> bool {
          uint initial_capacity = 1;
          ValueT *array = (ValueT *)m_allocator.allocate(sizeof(ValueT) * initial_capacity,
                                                         alignof(ValueT));
          new (array) ValueT(std::forward<ForwardValueT>(value));
          r_entry->ptr = array;
          r_entry->length = 1;
          r_entry->capacity = initial_capacity;
          return true;
        },
        /* Append new value for existing key. */
        [&](Entry *entry) -> bool {
          if (entry->length < entry->capacity) {
            new (entry->ptr + entry->length) ValueT(std::forward<ForwardValueT>(value));
            entry->length++;
          }
          else {
            uint old_capacity = entry->capacity;
            BLI_assert(old_capacity >= 1);
            uint new_capacity = old_capacity * 2;
            ValueT *new_array = (ValueT *)m_allocator.allocate(sizeof(ValueT) * new_capacity,
                                                               alignof(ValueT));
            uninitialized_relocate_n(entry->ptr, old_capacity, new_array);
            new (new_array + entry->length) ValueT(std::forward<ForwardValueT>(value));
            entry->ptr = new_array;
            entry->length++;
            entry->capacity = new_capacity;
          }
          return false;
        });
    return newly_inserted;
  }
};

} /* namespace BLI */
