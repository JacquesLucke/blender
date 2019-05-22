#pragma once

/* An unordered map implementation with small object optimization.
 * Similar to SmallSet, this builds on top of SmallVector
 * and ArrayLookup to reduce what this code has to deal with.
 */

#include "BLI_small_vector.hpp"
#include "BLI_array_lookup.hpp"
#include "BLI_array_ref.hpp"

namespace BLI {

template<typename K, typename V, uint N = 4> class SmallMap {
 private:
  struct Entry {
    K key;
    V value;
  };

  static const K &get_key_from_entry(const Entry &entry)
  {
    return entry.key;
  }

  SmallVector<Entry, N> m_entries;
  ArrayLookup<K, N, Entry, get_key_from_entry> m_lookup;

 public:
  class ValueIterator;

  SmallMap() = default;

  bool add(const K &key, const V &value)
  {
    uint potential_index = m_entries.size();
    bool newly_inserted = m_lookup.add(m_entries.begin(), key, potential_index);
    if (newly_inserted) {
      m_entries.append({key, value});
    }
    return newly_inserted;
  }

  void add_new(const K &key, const V &value)
  {
    BLI_assert(!this->contains(key));
    uint index = m_entries.size();
    m_entries.append({key, value});
    m_lookup.add_new(m_entries.begin(), index);
  }

  bool contains(const K &key) const
  {
    return m_lookup.contains(m_entries.begin(), key);
  }

  V pop(const K &key)
  {
    BLI_assert(this->contains(key));
    uint index = m_lookup.remove(m_entries.begin(), key);
    V value = std::move(m_entries[index].value);

    uint last_index = m_entries.size() - 1;
    if (index == last_index) {
      m_entries.remove_last();
    }
    else {
      m_entries.remove_and_reorder(index);
      K &moved_key = m_entries[index].key;
      m_lookup.update_index(moved_key, last_index, index);
    }
    return value;
  }

  V lookup(const K &key) const
  {
    return this->lookup_ref(key);
  }

  V lookup_default(const K &key, V default_value) const
  {
    V *ptr = this->lookup_ptr(key);
    if (ptr != nullptr) {
      return *ptr;
    }
    else {
      return default_value;
    }
  }

  V &lookup_ref(const K &key) const
  {
    V *ptr = this->lookup_ptr(key);
    BLI_assert(ptr);
    return *ptr;
  }

  V *lookup_ptr(const K &key) const
  {
    int index = m_lookup.find(m_entries.begin(), key);
    if (index >= 0) {
      return &m_entries[index].value;
    }
    else {
      return nullptr;
    }
  }

  V *lookup_ptr_or_insert(const K &key, V initial_value)
  {
    V *ptr = this->lookup_ptr(key);
    if (ptr == nullptr) {
      this->add_new(key, initial_value);
      ptr = &m_entries[m_entries.size() - 1].value;
    }
    return ptr;
  }

  template<typename... Args>
  V &lookup_ref_or_insert_func(const K &key, V (*create_value)(Args... args), Args &&... args)
  {
    V *value = this->lookup_ptr(key);
    if (value != NULL) {
      return *value;
    }
    this->add_new(key, create_value(std::forward<Args>(args)...));
    return this->lookup_ref(key);
  }

  V &lookup_ref_or_insert_key_func(const K &key, V (*create_value)(const K &key))
  {
    return lookup_ref_or_insert_func(key, create_value, key);
  }

  uint size() const
  {
    return m_entries.size();
  }

  void print_lookup_stats() const
  {
    m_lookup.print_lookup_stats(m_entries.begin());
  }

  /* Iterators
   **************************************************/

  static V &get_value_from_entry(Entry &entry)
  {
    return entry.value;
  }

  MappedArrayRef<Entry, V &, get_value_from_entry> values() const
  {
    return MappedArrayRef<Entry, V &, get_value_from_entry>(m_entries.begin(), m_entries.size());
  }

  static const K &get_key_from_entry(Entry &entry)
  {
    return entry.key;
  }

  MappedArrayRef<Entry, const K &, get_key_from_entry> keys() const
  {
    return MappedArrayRef<Entry, const K &, get_key_from_entry>(m_entries.begin(),
                                                                m_entries.size());
  }

  struct KeyValuePair {
    const K &key;
    V &value;
  };

  static KeyValuePair get_key_value_pair_from_entry(Entry &entry)
  {
    return {entry.key, entry.value};
  }

  MappedArrayRef<Entry, KeyValuePair, get_key_value_pair_from_entry> items() const
  {
    return MappedArrayRef<Entry, KeyValuePair, get_key_value_pair_from_entry>(m_entries.begin(),
                                                                              m_entries.size());
  }
};
};  // namespace BLI
