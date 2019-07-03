#pragma once

/* A map that allows storing multiple values per key.
 *
 * Values per key are stored in an array without being
 * able to efficiently check if a specific value exists
 * for a key. A linear search through all values for
 * a key has to be performed. When the number of values
 * per key is expected to be small, this is still fast.
 */

#include "BLI_small_map.hpp"
#include "BLI_multipool.hpp"
#include "BLI_array_ref.hpp"

namespace BLI {

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
