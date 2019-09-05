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
 * This vector wraps an externally provided memory buffer. At allows using any buffer as if it were
 * a vector. It does not grow the array dynamically. It asserts that the amount of added elements
 * does not exceed the capacity.
 *
 * This constraint allows a very efficient append operation, because no boundary checks have to be
 * performed in release builds.
 */

#pragma once

#include "BLI_array_ref.hpp"
#include "BLI_vector_adaptor.hpp"

namespace BLI {

template<typename T> class VectorAdaptor {
 private:
  T *m_begin;
  T *m_end;
  T *m_capacity_end;

 public:
  /**
   * Construct an empty vector adaptor.
   */
  VectorAdaptor() : m_begin(nullptr), m_end(nullptr), m_capacity_end(nullptr)
  {
  }

  /**
   * Construct using any pointer and a capacity.
   * The initial size is set to zero.
   */
  VectorAdaptor(T *ptr, uint capacity, uint size = 0)
      : m_begin(ptr), m_end(ptr + size), m_capacity_end(ptr + capacity)
  {
  }

  /**
   * Construct from an array. The capacity is automatically determined
   * from the length of the array.
   * The initial size is set to zero.
   */
  template<uint N>
  VectorAdaptor(T (&array)[N]) : m_begin(array), m_end(array), m_capacity_end(array + N)
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
    m_end = m_begin;
  }

  /**
   * Insert one element at the end of the vector.
   * Asserts, when the capacity is exceeded.
   */
  void append(const T &value)
  {
    BLI_assert(this->size() < this->capacity());
    new (m_end) T(value);
    m_end += 1;
  }

  void append(T &&value)
  {
    BLI_assert(this->size() < this->capacity());
    new (m_end) T(std::move(value));
    m_end += 1;
  }

  void append_n_times(const T &value, uint n)
  {
    BLI_assert(this->size() < this->capacity());
    uninitialized_fill_n(m_end, n, value);
    m_end += n;
  }

  /**
   * Insert multiple elements at the end of the vector.
   * Asserts, when the capacity is exceeded.
   */
  void extend(ArrayRef<T> values)
  {
    BLI_assert(this->size() + values.size() < this->capacity());
    std::uninitialized_copy_n(values.begin(), values.size(), m_end);
    m_end += values.size();
  }

  /**
   * Return the maximum size of the vector.
   */
  uint capacity() const
  {
    return m_capacity_end - m_begin;
  }

  /**
   * Return the current size of the vector.
   */
  uint size() const
  {
    return m_end - m_begin;
  }

  bool is_full() const
  {
    return m_end == m_capacity_end;
  }

  operator ArrayRef<T>() const
  {
    return ArrayRef<T>(m_begin, this->size());
  }

  T &operator[](uint index)
  {
    BLI_assert(index < this->size());
    return m_begin[index];
  }

  const T &operator[](uint index) const
  {
    BLI_assert(index < this->size());
    return m_begin[index];
  }

  T *begin()
  {
    return m_begin;
  }

  T *end()
  {
    return m_end;
  }

  const T *begin() const
  {
    return m_begin;
  }

  const T *end() const
  {
    return m_end;
  }
};

}  // namespace BLI
