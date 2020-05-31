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
#ifndef __BLI_ARRAY_HH__
#define __BLI_ARRAY_HH__

/** \file
 * \ingroup bli
 *
 * A `BLI::Array<T>` is a container for a fixed size array. Other than `std::array<T, N>`, the size
 * does not have to be known at compile time. If it is known, std::array should probably be used
 * instead. BLI::Array also supports small object optimization. That makes it more efficient when
 * the size turns out to be small at run-time.
 *
 * BLI::Array should be used instead of BLI::Vector whenever the size of the array is known at
 * construction time. Note however, that BLI::Array will default construct all elements when
 * initialized with the size-constructor. For trivial types, this is a noop, but it can add
 * overhead in general. If this becomes a problem, a different constructor which does not do
 * default construction can be added.
 *
 * A main benefit of using Array over Vector is that it expresses the intend of the developer
 * better. It indicates that the size of the data structure is not expected to change.
 */

#include "BLI_allocator.hh"
#include "BLI_array_ref.hh"
#include "BLI_index_range.hh"
#include "BLI_memory_utils.hh"
#include "BLI_utildefines.h"

namespace BLI {

template<
    /**
     * The type of the values stored in the array.
     */
    typename T,
    /**
     * The number of values that can be stored in the array, without doing a heap allocation.
     *
     * When T is large, the small buffer optimization is disabled by default to avoid large
     * unexpected allocations on the stack. It can still be enabled explicitely though.
     */
    uint InlineBufferCapacity = (sizeof(T) < 100) ? 4 : 0,
    /**
     * The allocator used by this array. Should rarely be changed, except when you don't want that
     * MEM_mallocN etc. is used internally.
     */
    typename Allocator = GuardedAllocator>
class Array {
 private:
  /** The beginning of the array. It might point into the inline buffer. */
  T *m_data;

  /** Number of elements in the array. */
  uint m_size;

  /** Used for allocations when the inline buffer is too small. */
  Allocator m_allocator;

  /** A placeholder buffer that will remain uninitialized until it is used. */
  AlignedBuffer<sizeof(T) * InlineBufferCapacity, alignof(T)> m_inline_buffer;

 public:
  /**
   * By default an empty array is created.
   */
  Array()
  {
    m_data = this->inline_buffer();
    m_size = 0;
  }

  /**
   * Create a new array that contains copies of all values.
   */
  Array(ArrayRef<T> values)
  {
    m_size = values.size();
    m_data = this->get_buffer_for_size(values.size());
    uninitialized_copy_n(values.begin(), m_size, m_data);
  }

  /**
   * Create a new array that contains copies of all values.
   */
  Array(const std::initializer_list<T> &values) : Array(ArrayRef<T>(values))
  {
  }

  /**
   * Create a new array with the given size. All values will be default constructed. For trivial
   * types like int, default construction is a noop.
   */
  explicit Array(uint size)
  {
    m_size = size;
    m_data = this->get_buffer_for_size(size);

    for (uint i = 0; i < m_size; i++) {
      new (m_data + i) T;
    }
  }

  /**
   * Create a new array with the given size. All values will be initialized by copying the given
   * default.
   */
  Array(uint size, const T &value)
  {
    m_size = size;
    m_data = this->get_buffer_for_size(size);
    uninitialized_fill_n(m_data, m_size, value);
  }

  Array(const Array &other)
  {
    m_size = other.size();
    m_allocator = other.m_allocator;

    m_data = this->get_buffer_for_size(other.size());
    uninitialized_copy_n(other.begin(), m_size, m_data);
  }

  Array(Array &&other) noexcept
  {
    m_size = other.m_size;
    m_allocator = other.m_allocator;

    if (!other.uses_inline_buffer()) {
      m_data = other.m_data;
    }
    else {
      m_data = this->get_buffer_for_size(m_size);
      uninitialized_relocate_n(other.m_data, m_size, m_data);
    }

    other.m_data = other.inline_buffer();
    other.m_size = 0;
  }

  ~Array()
  {
    destruct_n(m_data, m_size);
    if (!this->uses_inline_buffer()) {
      m_allocator.deallocate((void *)m_data);
    }
  }

  Array &operator=(const Array &other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Array();
    new (this) Array(other);
    return *this;
  }

  Array &operator=(Array &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Array();
    new (this) Array(std::move(other));
    return *this;
  }

  operator ArrayRef<T>() const
  {
    return ArrayRef<T>(m_data, m_size);
  }

  operator MutableArrayRef<T>()
  {
    return MutableArrayRef<T>(m_data, m_size);
  }

  ArrayRef<T> as_ref() const
  {
    return *this;
  }

  MutableArrayRef<T> as_mutable_ref()
  {
    return *this;
  }

  T &operator[](uint index)
  {
    BLI_assert(index < m_size);
    return m_data[index];
  }

  const T &operator[](uint index) const
  {
    BLI_assert(index < m_size);
    return m_data[index];
  }

  /**
   * Returns the number of elements in the array.
   */
  uint size() const
  {
    return m_size;
  }

  /**
   * Copies the value to all indices in the array.
   */
  void fill(const T &value)
  {
    MutableArrayRef<T>(*this).fill(value);
  }

  /**
   * Copies the value to the given indices in the array.
   */
  void fill_indices(ArrayRef<uint> indices, const T &value)
  {
    MutableArrayRef<T>(*this).fill_indices(indices, value);
  }

  const T *begin() const
  {
    return m_data;
  }

  const T *end() const
  {
    return m_data + m_size;
  }

  T *begin()
  {
    return m_data;
  }

  T *end()
  {
    return m_data + m_size;
  }

  /**
   * Get an index range containing all valid indices for this array.
   */
  IndexRange index_range() const
  {
    return IndexRange(m_size);
  }

  /**
   * Sets the size to zero. This should be used carefully to avoid memory leaks.
   */
  void clear_without_destruct()
  {
    m_size = 0;
  }

  /**
   * Access the allocator used by this array.
   */
  Allocator &allocator()
  {
    return m_allocator;
  }

  /**
   * Get the value of the InlineBufferCapacity template argument. This is the number of elements
   * that can be stored without doing an allocation.
   */
  static uint inline_buffer_capacity()
  {
    return InlineBufferCapacity;
  }

 private:
  T *get_buffer_for_size(uint size)
  {
    if (size <= InlineBufferCapacity) {
      return this->inline_buffer();
    }
    else {
      return this->allocate(size);
    }
  }

  T *inline_buffer() const
  {
    return (T *)m_inline_buffer.ptr();
  }

  T *allocate(uint size)
  {
    return (T *)m_allocator.allocate_aligned(size * sizeof(T), alignof(T), __func__);
  }

  bool uses_inline_buffer() const
  {
    return m_data == this->inline_buffer();
  }
};

}  // namespace BLI

#endif /* __BLI_ARRAY_HH__ */
