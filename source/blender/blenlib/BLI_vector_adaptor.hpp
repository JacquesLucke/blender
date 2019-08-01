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
 * This vector wraps an externally provided memory buffer.
 * At allows using any buffer as if it were an array.
 * It does not grow the array dynamically. If the number of
 * elements is about to exceed the capacity, it asserts false.
 *
 * This constraint allows a very efficient append operation,
 * since no boundary checks have to be performed in release builds.
 */

#pragma once

#include "BLI_array_ref.hpp"
#include "BLI_vector_adaptor.hpp"

namespace BLI {

template<typename T> class VectorAdaptor {
 private:
  T *m_start;
  T *m_end;
  uint m_capacity;

 public:
  /**
   * Construct an empty vector adaptor.
   */
  VectorAdaptor() : m_start(nullptr), m_end(nullptr), m_capacity(0)
  {
  }

  /**
   * Construct using any pointer and a capacity.
   * The initial size is set to zero.
   */
  VectorAdaptor(T *ptr, uint capacity, uint size = 0)
      : m_start(ptr), m_end(ptr + size), m_capacity(capacity)
  {
  }

  /**
   * Construct from an array. The capacity is automatically determined
   * from the length of the array.
   * The initial size is set to zero.
   */
  template<uint N> VectorAdaptor(T (&array)[N]) : m_start(array), m_end(array), m_capacity(N)
  {
  }

  /**
   * Elements should continue to live after the adapter is destructed.
   */
  ~VectorAdaptor() = default;

  void clear()
  {
    for (T &value : *this) {
      value.~T();
    }
    m_end = m_start;
  }

  /**
   * Insert one element at the end of the vector.
   * Asserts, when the capacity is exceeded.
   */
  void append(const T &value)
  {
    BLI_assert(this->size() < m_capacity);
    std::uninitialized_copy_n(&value, 1, m_end);
    m_end += 1;
  }

  void append(T &&value)
  {
    BLI_assert(this->size() < m_capacity);
    std::uninitialized_copy_n(std::make_move_iterator(&value), 1, m_end);
    m_end += 1;
  }

  /**
   * Insert multiple elements at the end of the vector.
   * Asserts, when the capacity is exceeded.
   */
  void extend(ArrayRef<T> values)
  {
    BLI_assert(this->size() + values.size() < m_capacity);
    std::uninitialized_copy_n(values.begin(), values.size(), m_end);
    m_end += values.size();
  }

  /**
   * Return the maximum size of the vector.
   */
  uint capacity() const
  {
    return m_capacity;
  }

  /**
   * Return the current size of the vector.
   */
  uint size() const
  {
    return m_end - m_start;
  }

  bool is_full() const
  {
    return m_capacity == this->size();
  }

  operator ArrayRef<T>() const
  {
    return ArrayRef<T>(m_start, this->size());
  }

  T &operator[](uint index)
  {
    BLI_assert(index < this->size());
    return this->begin()[index];
  }

  T *begin() const
  {
    return m_start;
  }

  T *end() const
  {
    return m_end;
  }
};

}  // namespace BLI
