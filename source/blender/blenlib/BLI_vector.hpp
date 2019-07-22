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
 * This vector wraps a dynamically sized array of a specific type.
 * It supports small object optimization. That means, when the
 * vector only contains a few elements, no extra memory allocation
 * is performed. Instead, those elements are stored directly in
 * the vector.
 */

#pragma once

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

#include "BLI_utildefines.h"
#include "BLI_array_ref.hpp"
#include "BLI_listbase_wrapper.hpp"
#include "BLI_math_base.h"

#include "MEM_guardedalloc.h"

namespace BLI {

template<typename T> void uninitialized_relocate_n(T *src, uint n, T *dst)
{
  std::uninitialized_copy_n(std::make_move_iterator(src), n, dst);
  for (uint i = 0; i < n; i++) {
    src[i].~T();
  }
}

template<typename T, uint N = 4> class Vector {
 private:
  T *m_elements;
  uint m_size = 0;
  uint m_capacity = N;
  char m_small_buffer[sizeof(T) * N];

 public:
  /**
   * Create an empty vector.
   * This does not do any memory allocation.
   */
  Vector()
  {
    m_elements = this->small_buffer();
    m_capacity = N;
    m_size = 0;
  }

  /**
   * Create a vector with a specific size.
   * The elements will be default initialized.
   */
  Vector(uint size) : Vector()
  {
    this->reserve(size);
    for (uint i = 0; i < size; i++) {
      new (this->element_ptr(i)) T();
    }
    m_size = size;
  }

  /**
   * Create a vector filled with a specific value.
   */
  Vector(uint size, const T &value) : Vector()
  {
    this->reserve(size);
    std::fill_n(m_elements, size, value);
    m_size = size;
  }

  /**
   * Create a vector from an initializer list.
   */
  Vector(std::initializer_list<T> values) : Vector(ArrayRef<T>(values))
  {
  }

  /**
   * Create a vector from an array ref.
   */
  Vector(ArrayRef<T> values) : Vector()
  {
    this->reserve(values.size());
    std::uninitialized_copy_n(values.begin(), values.size(), this->begin());
    m_size = values.size();
  }

  /**
   * Create a vector from any container. It must be possible to use the container in a range-for
   * loop.
   */
  template<typename ContainerT> static Vector FromContainer(const ContainerT &container)
  {
    Vector vector;
    for (const auto &value : container) {
      vector.append(value);
    }
    return vector;
  }

  /**
   * Create a vector from a mapped array ref. This can e.g. be used to create vectors from
   * map.keys() for map.values().
   */
  template<typename ArrayT, typename ValueT, ValueT (*GetValue)(ArrayT &item)>
  Vector(MappedArrayRef<ArrayT, ValueT, GetValue> values) : Vector()
  {
    this->reserve(values.size());
    for (uint i = 0; i < values.size(); i++) {
      std::uninitialized_copy_n(&values[i], 1, m_elements + i);
    }
    m_size = values.size();
  }

  /**
   * Create a vector from a ListBase.
   */
  Vector(ListBase &values, bool intrusive_next_and_prev_pointers) : Vector()
  {
    if (intrusive_next_and_prev_pointers) {
      for (T value : ListBaseWrapper<T, true>(values)) {
        this->append(value);
      }
    }
    else {
      for (T value : ListBaseWrapper<T, false>(values)) {
        this->append(value);
      }
    }
  }

  /**
   * Create a copy of another vector.
   * The other vector will not be changed.
   * If the other vector has less than N elements, no allocation will be made.
   */
  Vector(const Vector &other)
  {
    this->copy_from_other(other);
  }

  /**
   * Steal the elements from another vector.
   * This does not do an allocation.
   * The other vector will have zero elements afterwards.
   */
  Vector(Vector &&other)
  {
    this->steal_from_other(other);
  }

  ~Vector()
  {
    this->destruct_elements_and_free_memory();
  }

  operator ArrayRef<T>() const
  {
    return ArrayRef<T>(m_elements, m_size);
  }

  Vector &operator=(const Vector &other)
  {
    if (this == &other) {
      return *this;
    }

    this->destruct_elements_and_free_memory();
    this->copy_from_other(other);

    return *this;
  }

  Vector &operator=(Vector &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->destruct_elements_and_free_memory();
    this->steal_from_other(other);

    return *this;
  }

  /**
   * Make sure that enough memory is allocated to hold size elements.
   * This won't necessarily make an allocation when size is small.
   * The actual size of the vector does not change.
   */
  void reserve(uint size)
  {
    this->grow(size);
  }

  /**
   * Afterwards the vector has 0 elements, but will still have
   * memory to be refilled again.
   */
  void clear()
  {
    this->destruct_elements_but_keep_memory();
    m_size = 0;
  }

  /**
   * Afterwards the vector has 0 elements and any allocated memory
   * will be freed.
   */
  void clear_and_make_small()
  {
    this->destruct_elements_and_free_memory();
    m_size = 0;
    m_elements = this->small_buffer();
  }

  /**
   * Insert a new element at the end of the vector.
   * This might cause a reallocation with the capacity is exceeded.
   */
  void append(const T &value)
  {
    this->ensure_space_for_one();
    this->append_unchecked(value);
  }

  void append(T &&value)
  {
    this->ensure_space_for_one();
    this->append_unchecked(std::move(value));
  }

  void append_unchecked(const T &value)
  {
    BLI_assert(m_size < m_capacity);
    std::uninitialized_copy_n(&value, 1, this->end());
    m_size++;
  }

  void append_unchecked(T &&value)
  {
    BLI_assert(m_size < m_capacity);
    std::uninitialized_copy_n(std::make_move_iterator(&value), 1, this->end());
    m_size++;
  }

  /**
   * Insert the same element n times at the end of the vector.
   * This might result in a reallocation internally.
   */
  void append_n_times(const T &value, uint n)
  {
    this->reserve(m_size + n);
    std::uninitialized_fill_n(this->end(), n, value);
    m_size += n;
  }

  void increase_size_unchecked(uint n)
  {
    BLI_assert(m_size + n <= m_capacity);
    m_size += n;
  }

  /**
   * Copy the elements of another vector to the end of this vector.
   */
  void extend(const Vector &other)
  {
    this->extend(other.begin(), other.size());
  }

  void extend(ArrayRef<T> array)
  {
    this->extend(array.begin(), array.size());
  }

  void extend(const T *start, uint amount)
  {
    this->reserve(m_size + amount);
    this->extend_unchecked(start, amount);
  }

  void extend_unchecked(ArrayRef<T> array)
  {
    this->extend_unchecked(array.begin(), array.size());
  }

  void extend_unchecked(const T *start, uint amount)
  {
    BLI_assert(m_size + amount <= m_capacity);
    std::uninitialized_copy_n(start, amount, this->end());
    m_size += amount;
  }

  /**
   * Return a reference to the last element in the vector.
   * This will assert when the vector is empty.
   */
  T &last() const
  {
    BLI_assert(m_size > 0);
    return m_elements[m_size - 1];
  }

  /**
   * Replace every element with a new value.
   */
  void fill(const T &value)
  {
    for (uint i = 0; i < m_size; i++) {
      m_elements[i] = value;
    }
  }

  /**
   * Return how many values are currently stored in the vector.
   */
  uint size() const
  {
    return m_size;
  }

  /**
   * Returns true when the vector contains no elements, otherwise false.
   */
  bool empty() const
  {
    return this->size() == 0;
  }

  /**
   * Deconstructs the last element and decreases the size by one.
   * This will assert when the vector is empty.
   */
  void remove_last()
  {
    BLI_assert(!this->empty());
    this->destruct_element(m_size - 1);
    m_size--;
  }

  /**
   * Remove the last element from the vector and return it.
   */
  T pop_last()
  {
    BLI_assert(!this->empty());
    T value = m_elements[this->size() - 1];
    this->remove_last();
    return value;
  }

  /**
   * Delete any element in the vector.
   * The empty space will be filled by the previously last element.
   */
  void remove_and_reorder(uint index)
  {
    BLI_assert(this->is_index_in_range(index));
    if (index < m_size - 1) {
      /* Move last element to index. */
      std::copy(std::make_move_iterator(this->end() - 1),
                std::make_move_iterator(this->end()),
                this->element_ptr(index));
    }
    this->destruct_element(m_size - 1);
    m_size--;
  }

  /**
   * Do a linear search to find the value in the vector.
   * When found, return the first index, otherwise return -1.
   */
  int index(const T &value) const
  {
    for (uint i = 0; i < m_size; i++) {
      if (m_elements[i] == value) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Do a linear search to see of the value is in the vector.
   * Return true when it exists, otherwise false.
   */
  bool contains(const T &value) const
  {
    return this->index(value) != -1;
  }

  /**
   * Compare vectors element-wise.
   * Return true when they have the same length and all elements
   * compare equal, otherwise false.
   */
  static bool all_equal(const Vector &a, const Vector &b)
  {
    if (a.size() != b.size()) {
      return false;
    }
    for (uint i = 0; i < a.size(); i++) {
      if (a[i] != b[i]) {
        return false;
      }
    }
    return true;
  }

  T &operator[](const int index) const
  {
    BLI_assert(this->is_index_in_range(index));
    return m_elements[index];
  }

  T *begin() const
  {
    return m_elements;
  }
  T *end() const
  {
    return this->begin() + this->size();
  }

  const T *cbegin() const
  {
    return this->begin();
  }
  const T *cend() const
  {
    return this->end();
  }

  void print_stats() const
  {
    std::cout << "Small Vector at " << (void *)this << ":" << std::endl;
    std::cout << "  Elements: " << this->size() << std::endl;
    std::cout << "  Capacity: " << this->m_capacity << std::endl;
    std::cout << "  Small Elements: " << N << "  Size on Stack: " << sizeof(*this) << std::endl;
  }

 private:
  T *small_buffer() const
  {
    return (T *)m_small_buffer;
  }

  bool is_small() const
  {
    return m_elements == this->small_buffer();
  }

  bool is_index_in_range(uint index) const
  {
    return index < this->size();
  }

  T *element_ptr(uint index) const
  {
    return m_elements + index;
  }

  inline void ensure_space_for_one()
  {
    if (m_size >= m_capacity) {
      this->grow(std::max(m_capacity * 2, (uint)1));
    }
  }

  void grow(uint min_capacity)
  {
    if (m_capacity >= min_capacity) {
      return;
    }

    /* Round up to the next power of two. Otherwise consecutive calls to grow can cause a
     * reallocation every time even though the min_capacity only increments. */
    min_capacity = power_of_2_max_u(min_capacity);

    m_capacity = min_capacity;

    T *new_array = (T *)MEM_malloc_arrayN(m_capacity, sizeof(T), __func__);
    uninitialized_relocate_n(m_elements, m_size, new_array);

    if (!this->is_small()) {
      MEM_freeN(m_elements);
    }

    m_elements = new_array;
  }

  void copy_from_other(const Vector &other)
  {
    if (other.is_small()) {
      m_elements = this->small_buffer();
    }
    else {
      m_elements = (T *)MEM_malloc_arrayN(other.m_capacity, sizeof(T), __func__);
    }

    std::uninitialized_copy(other.begin(), other.end(), m_elements);
    m_capacity = other.m_capacity;
    m_size = other.m_size;
  }

  void steal_from_other(Vector &other)
  {
    if (other.is_small()) {
      uninitialized_relocate_n(other.begin(), other.size(), this->small_buffer());
      m_elements = this->small_buffer();
    }
    else {
      m_elements = other.m_elements;
    }

    m_capacity = other.m_capacity;
    m_size = other.m_size;

    other.m_size = 0;
    other.m_capacity = N;
    other.m_elements = other.small_buffer();
  }

  void destruct_elements_and_free_memory()
  {
    this->destruct_elements_but_keep_memory();
    if (!this->is_small()) {
      MEM_freeN(m_elements);
    }
  }

  void destruct_elements_but_keep_memory()
  {
    for (uint i = 0; i < m_size; i++) {
      this->destruct_element(i);
    }
  }

  void destruct_element(uint index)
  {
    this->element_ptr(index)->~T();
  }
};

} /* namespace BLI */
