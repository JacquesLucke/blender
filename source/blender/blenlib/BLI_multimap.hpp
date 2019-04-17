#pragma once

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

  bool contains(const K &key) const
  {
    return m_map.contains(key);
  }

  ArrayRef<V> lookup(const K &key) const
  {
    BLI_assert(this->contains(key));
    Entry &entry = m_map.lookup_ref(key);
    return ArrayRef<V>(entry.ptr, entry.length);
  }
};

} /* namespace BLI */
