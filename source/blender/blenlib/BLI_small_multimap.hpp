#pragma once

/* A multimap is a map that allows storing multiple values per key. */

#include "BLI_small_map.hpp"
#include "BLI_array_ref.hpp"

namespace BLI {

template<typename K, typename V, uint N = 4> class SmallMultiMap {
 private:
  struct Entry {
    uint offset;
    uint length;
    uint capacity;

    ArrayRef<V> get_slice(ArrayRef<V> array)
    {
      return array.slice(offset, length);
    }
  };

  SmallMap<K, Entry, N> m_map;
  SmallVector<V, N> m_elements;

 public:
  SmallMultiMap() = default;

  uint key_amount() const
  {
    return m_map.size();
  }

  uint value_amount(const K &key) const
  {
    return this->lookup_default(key).size();
  }

  bool add(const K &key, const V &value)
  {
    BLI_STATIC_ASSERT(std::is_trivially_destructible<V>::value, "");
    bool newly_inserted = m_map.insert_or_modify(
        key,
        /* Insert new key with value. */
        [this, &key, &value]() -> Entry {
          uint offset = m_elements.size();
          m_elements.append(value);
          return {offset, 1, 1};
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

  void add_new(const K &key, const V &value)
  {
    BLI_assert(!m_map.contains(key));
    uint offset = m_elements.size();
    m_elements.append(value);
    m_map.add_new(key, {offset, 1, 1});
  }

  void add_multiple_new(const K &key, ArrayRef<V> values)
  {
    BLI_assert(!m_map.contains(key));
    uint offset = m_elements.size();
    m_elements.extend(values);
    m_map.add_new(key, {offset, values.size(), values.size()});
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

  typename SmallMap<K, Entry>::KeysReturnT keys() const
  {
    return m_map.keys();
  }
};

} /* namespace BLI */
