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
 * A unordered set implementation that supports small object optimization.
 * It builds on top of SmallVector and ArrayLookup, so that
 * it does not have to deal with memory management and the
 * details of the hashing and probing algorithm.
 */

#pragma once

#include "BLI_small_vector.hpp"
#include "BLI_array_lookup.hpp"
#include "BLI_array_ref.hpp"

namespace BLI {

template<typename T, uint N = 4> class Set {
 protected:
  SmallVector<T, N> m_elements;
  ArrayLookup<T, N> m_lookup;

 public:
  /**
   * Create an empty set.
   */
  Set() = default;

  /**
   * Create a set that contains any of the given values at least once.
   * The size of the set might be small than the original array.
   */
  Set(ArrayRef<T> values)
  {
    this->add_multiple(values);
  }

  Set(const SmallVector<T> &values) : Set(ArrayRef<T>(values))
  {
  }

  Set(const std::initializer_list<T> &values) : Set(ArrayRef<T>(values))
  {
  }

  /**
   * Return the number elements in the set.
   */
  uint size() const
  {
    return m_elements.size();
  }

  /**
   * Return true when the value is in the set, otherwise false.
   */
  bool contains(const T &value) const
  {
    return m_lookup.contains(m_elements.begin(), value);
  }

  /**
   * Insert a value in the set, that was not there before.
   * This will assert when the value existed before.
   * This method might be faster than "add".
   * Furthermore, it should be used whenever applicable because it expresses the intent better.
   */
  void add_new(const T &value)
  {
    BLI_assert(!this->contains(value));
    uint index = m_elements.size();
    m_elements.append(value);
    m_lookup.add_new(m_elements.begin(), index);
  }

  /**
   * Insert the value in the set if it did not exist before.
   * Return false, when it existed before, otherwise true.
   */
  bool add(const T &value)
  {
    uint desired_new_index = m_elements.size();
    uint value_index = m_lookup.add(m_elements.begin(), value, desired_new_index);
    bool newly_inserted = value_index == desired_new_index;
    if (newly_inserted) {
      m_elements.append(value);
    }
    return newly_inserted;
  }

  /**
   * Insert multiple values in the set.
   * Any value that already exists will be skipped.
   */
  void add_multiple(ArrayRef<T> values)
  {
    for (T &value : values) {
      this->add(value);
    }
  }

  /**
   * Insert multiple values in the set.
   * Asserts when any of the values exists already.
   */
  void add_multiple_new(ArrayRef<T> values)
  {
    for (T &value : values) {
      this->add_new(value);
    }
  }

  /**
   * Remove and return any value from the set.
   */
  T pop()
  {
    BLI_assert(this->size() > 0);
    T value = m_elements.pop_last();
    uint index = m_elements.size();
    m_lookup.remove(value, index);
    return value;
  }

  /**
   * Remove the value from the set.
   * This expects that the value existed before and will assert otherwise.
   */
  void remove(const T &value)
  {
    BLI_assert(this->contains(value));
    uint index = m_lookup.remove(m_elements.begin(), value);

    uint last_index = m_elements.size() - 1;
    if (index == last_index) {
      m_elements.remove_last();
    }
    else {
      m_elements.remove_and_reorder(index);
      T &moved_value = m_elements[index];
      m_lookup.update_index(moved_value, last_index, index);
    }
  }

  /**
   * Return any of the elements of the set.
   */
  T any() const
  {
    BLI_assert(this->size() > 0);
    return m_elements[0];
  }

  /**
   * Convert all values in the set into a vector.
   */
  SmallVector<T> to_small_vector() const
  {
    return m_elements;
  }

  /**
   * Return true when there is no value that exists in both sets, otherwise false.
   */
  static bool Disjoint(const Set &a, const Set &b)
  {
    return !Set::Intersects(a, b);
  }

  /**
   * Return true when there is at least one value that exists in both sets, otherwise false.
   */
  static bool Intersects(const Set &a, const Set &b)
  {
    for (const T &value : a) {
      if (b.contains(value)) {
        return true;
      }
    }
    return false;
  }

  T *begin() const
  {
    return m_elements.begin();
  }

  T *end() const
  {
    return m_elements.end();
  }

  void print_lookup_stats()
  {
    m_lookup.print_lookup_stats(m_elements.begin());
  }
};

} /* namespace BLI */
