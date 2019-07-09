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
 * An unordered map implementation with small object optimization.
 * Similar to SmallSet, this builds on top of SmallVector
 * and ArrayLookup to reduce what this code has to deal with.
 */

#pragma once

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

  /**
   * Create an empty map.
   */
  SmallMap() = default;

  /**
   * Insert a key-value pair in the map, when the key does not exist.
   * Return true when the pair has been newly inserted, otherwise false.
   */
  bool add(const K &key, const V &value)
  {
    uint desired_new_index = m_entries.size();
    uint value_index = m_lookup.add(m_entries.begin(), key, desired_new_index);
    bool newly_inserted = value_index == desired_new_index;
    if (newly_inserted) {
      m_entries.append({key, value});
    }
    return newly_inserted;
  }

  /**
   * Insert a new key-value pair in the map.
   * This will assert when the key exists already.
   */
  void add_new(const K &key, const V &value)
  {
    BLI_assert(!this->contains(key));
    uint index = m_entries.size();
    m_entries.append({key, value});
    m_lookup.add_new(m_entries.begin(), index);
  }

  /**
   * Insert a new key-value pair in the map. If the key exists already, the value will be
   * overriden.
   * Returns when the value was newly inserted, otherwise false.
   */
  bool add_override(const K &key, const V &value)
  {
    return this->add_or_modify(
        key, [value]() { return value; }, [value](V &old_value) { old_value = value; });
  }

  /**
   * Return true when the key exists in the map, otherwise false.
   */
  bool contains(const K &key) const
  {
    return m_lookup.contains(m_entries.begin(), key);
  }

  /**
   * Remove the key-value pair identified by the key from the map.
   * Returns the corresponding value.
   * This will assert when the key does not exist.
   */
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

  /**
   * Return the value corresponding to the key.
   * This will assert when the key does not exist.
   */
  V lookup(const K &key) const
  {
    return this->lookup_ref(key);
  }

  /**
   * Return the value corresponding to the key.
   * If the key does not exist, return the given default value.
   */
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

  /**
   * Return a reference to the value corresponding to a key.
   * This will assert when the key does not exist.
   */
  V &lookup_ref(const K &key) const
  {
    V *ptr = this->lookup_ptr(key);
    BLI_assert(ptr);
    return *ptr;
  }

  /**
   * Return a pointer to the value corresponding to the key.
   * Returns nullptr when the key does not exist.
   */
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

  /**
   * Return a reference to the value corresponding to the key.
   * If the key does not exist yet, insert the given key-value-pair first.
   */
  V &lookup_ref_or_insert(const K &key, V initial_value)
  {
    V *ptr = this->lookup_ptr(key);
    if (ptr != nullptr) {
      return *ptr;
    }
    this->add_new(key, initial_value);
    return m_entries[m_entries.size() - 1].value;
  }

  /**
   * Return a reference to the value corresponding to the key.
   * If the key does not exist yet, create the value by calling the function, and insert the
   * key-value pair.
   */
  template<typename CreateValueFunc>
  V &lookup_ref_or_insert_func(const K &key, const CreateValueFunc &create_value)
  {
    V *ptr = this->lookup_ptr(key);
    if (ptr != nullptr) {
      return *ptr;
    }

    this->add_new(key, create_value());
    return m_entries[m_entries.size() - 1].value;
  }

  /**
   * Insert a new value for the given key or modify the existing one.
   * Return true when a new value was inserted, false otherwise.
   */
  template<typename CreateValueFunc, typename ModifyValueFunc>
  bool add_or_modify(const K &key,
                     const CreateValueFunc &create_value,
                     const ModifyValueFunc &modify_value)
  {
    uint desired_new_index = m_entries.size();
    uint value_index = m_lookup.add(m_entries.begin(), key, desired_new_index);
    if (value_index == desired_new_index) {
      m_entries.append({key, create_value()});
      return true;
    }
    else {
      modify_value(m_entries[value_index].value);
      return false;
    }
  }

  /**
   * Return the number of key-value pairs in the map.
   */
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

  using KeysReturnT = MappedArrayRef<Entry, const K &, get_key_from_entry>;
  KeysReturnT keys() const
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
