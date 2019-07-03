#pragma once

/* A multimap is a map that allows storing multiple values per key.
 *
 * The optimal data structure layout highly depends on the access pattern. For that reason, it can
 * make sense to have multiple implementation for similar queries.
 */

#include "BLI_small_map.hpp"
#include "BLI_multipool.hpp"
#include "BLI_array_ref.hpp"

namespace BLI {

/**
 * This stores an array per key. It should be used when the array per key changes rarely. All
 * arrays are just concatenated with some space in between to allow for growing arrays.
 * If an array is becomes too large, it is copied to the end, leaving a whole that will not be
 * filled again.
 */
template<typename K, typename V, uint N = 4> class ValueArrayMap {
 private:
  struct Entry {
    K key;
    uint offset;
    uint length;
    uint capacity;

    static const K &get_key(const Entry &entry)
    {
      BLI_STATIC_ASSERT(std::is_trivial<V>::value, "only works with trivial value types");
      return entry.key;
    }

    ArrayRef<V> get_slice(ArrayRef<V> array)
    {
      return array.slice(offset, length);
    }
  };

  SmallMap<K, Entry, N> m_map;
  SmallVector<V, N> m_elements;

 public:
  ValueArrayMap() = default;

  uint key_amount() const
  {
    return m_map.size();
  }

  bool add(const K &key, const V &value)
  {
    bool newly_inserted = m_map.insert_or_modify(
        key,
        /* Insert new key with value. */
        [this, &key, &value]() -> Entry {
          uint offset = m_elements.size();
          m_elements.append(value);
          return {key, offset, 1, 1};
        },
        /* Append new value for existing key. */
        [this, &value](Entry &entry) {
          if (entry.length < entry.capacity) {
            m_elements[entry.offset + entry.length] = value;
            entry.length += 1;
          }
          else {
            uint new_offset = m_elements.size();

            /* Copy the existing elements to the end. */
            m_elements.extend(entry.get_slice(m_elements));
            /* Insert the new value and reserve the capacity for this
             * entry. */
            m_elements.append_n_times(value, entry.length);

            entry.offset = new_offset;
            entry.length += 1;
            entry.capacity *= 2;
          }
        });
    return newly_inserted;
  }

  bool add_new(const K &key, const V &value)
  {
    BLI_assert(!m_map.contains(key));
    uint offset = m_elements.size();
    m_elements.append(value);
    m_map.add_new(key, {key, offset, 1, 1});
  }

  void add_multiple_new(const K &key, ArrayRef<V> values)
  {
    BLI_assert(!m_map.contains(key));
    for (const V &value : values) {
      this->add(key, value);
    }
  }

  ArrayRef<V> lookup(const K &key) const
  {
    BLI_assert(m_map.contains(key));
    return this->lookup_default(key);
  }

  ArrayRef<V> lookup_default(const K &key, ArrayRef<V> default_array = ArrayRef<V>()) const
  {
    Entry *entry = m_map.lookup_ptr(key);
    if (entry == nullptr) {
      return default_array;
    }
    else {
      return entry->get_slice(m_elements);
    }
  }

  bool contains(const K &key) const
  {
    return m_map.contains(key);
  }
};

template<typename K, typename V> class MultiMap {
 private:
  struct Entry {
    K key;
    V *ptr;
    uint length;
    uint capacity;

    Entry(K key, V *ptr, uint length, uint capacity)
        : key(key), ptr(ptr), length(length), capacity(capacity)
    {
    }
  };

  static const K &get_key_from_item(const Entry &entry)
  {
    return entry.key;
  }

  SmallMap<K, Entry> m_map;
  MemMultiPool m_pool;

 public:
  uint size() const
  {
    return m_map.size();
  }

  void add(const K &key, const V &value)
  {
    if (m_map.contains(key)) {
      Entry &entry = m_map.lookup_ref(key);
      if (entry.length == entry.capacity) {
        uint new_capacity = entry.capacity * 2;
        V *old_ptr = entry.ptr;
        V *new_ptr = m_pool.allocate_array<V>(new_capacity);
        uninitialized_relocate_n(old_ptr, entry.length, new_ptr);
        m_pool.deallocate(old_ptr);
        entry.ptr = new_ptr;
        entry.capacity = new_capacity;
      }
      std::uninitialized_copy_n(&value, 1, entry.ptr + entry.length);
      entry.length++;
    }
    else {
      this->add_new(key, value);
    }
  }

  void add_new(const K &key, const V &value)
  {
    BLI_assert(!m_map.contains(key));
    V *ptr = m_pool.allocate<V>();
    std::uninitialized_copy_n(&value, 1, ptr);
    m_map.add_new(key, Entry(key, ptr, 1, 1));
  }

  void add_multiple_new(const K &key, ArrayRef<V> values)
  {
    BLI_assert(!m_map.contains(key));
    uint amount = values.size();
    V *ptr = m_pool.allocate_array<V>(amount);
    std::uninitialized_copy_n(values.begin(), amount, ptr);
    m_map.add_new(key, Entry(key, ptr, amount, amount));
  }

  bool contains(const K &key) const
  {
    return m_map.contains(key);
  }

  bool has_at_least_one_value(const K &key) const
  {
    return this->values_for_key(key) >= 1;
  }

  uint values_for_key(const K &key) const
  {
    Entry *entry = m_map.lookup_ptr(key);
    if (entry == nullptr) {
      return 0;
    }
    else {
      return entry->length;
    }
  }

  ArrayRef<V> lookup(const K &key) const
  {
    BLI_assert(this->contains(key));
    Entry &entry = m_map.lookup_ref(key);
    return ArrayRef<V>(entry.ptr, entry.length);
  }

  ArrayRef<V> lookup_default(const K &key, ArrayRef<V> default_return = ArrayRef<V>()) const
  {
    Entry *entry = m_map.lookup_ptr(key);
    if (entry == nullptr) {
      return default_return;
    }
    else {
      return ArrayRef<V>(entry->ptr, entry->length);
    }
  }

  typename SmallMap<K, Entry>::KeysReturnT keys() const
  {
    return m_map.keys();
  }
};

} /* namespace BLI */
