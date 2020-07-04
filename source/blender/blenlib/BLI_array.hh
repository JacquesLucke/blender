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
 * A `blender::Array<T>` is a container for a fixed size array the size of which is NOT known at
 * compile time.
 *
 * If the size is known at compile time, `std::array<T, N>` should be used instead.
 *
 * blender::Array should usually be used instead of blender::Vector whenever the number of elements
 * is known at construction time. Note however, that blender::Array will default construct all
 * elements when initialized with the size-constructor. For trivial types, this does nothing. In
 * all other cases, this adds overhead.
 *
 * A main benefit of using Array over Vector is that it expresses the intent of the developer
 * better. It indicates that the size of the data structure is not expected to change. Furthermore,
 * you can be more certain that an array does not overallocate.
 *
 * blender::Array supports small object optimization to improve performance when the size turns out
 * to be small at run-time.
 */

#include "BLI_allocator.hh"
#include "BLI_index_range.hh"
#include "BLI_memory_utils.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

namespace blender {

template<
    /**
     * The type of the values stored in the array.
     */
    typename T,
    /**
     * The number of values that can be stored in the array, without doing a heap allocation.
     *
     * When T is large, the small buffer optimization is disabled by default to avoid large
     * unexpected allocations on the stack. It can still be enabled explicitly though.
     */
    uint InlineBufferCapacity = (sizeof(T) < 100) ? 4 : 0,
    /**
     * The allocator used by this array. Should rarely be changed, except when you don't want that
     * MEM_* functions are used internally.
     */
    typename Allocator = GuardedAllocator>
class Array {
 private:
  /** The beginning of the array. It might point into the inline buffer. */
  T *data_;

  /** Number of elements in the array. */
  uint size_;

  /** Used for allocations when the inline buffer is too small. */
  Allocator allocator_;

  /** A placeholder buffer that will remain uninitialized until it is used. */
  TypedAlignedBuffer<T, InlineBufferCapacity> inline_buffer_;

 public:
  /**
   * By default an empty array is created.
   */
  Array(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
    data_ = this->inline_buffer_;
    size_ = 0;
  }

  Array(NoExceptConstructor, Allocator allocator = {}) noexcept : Array(allocator)
  {
  }

  /**
   * Create a new array that contains copies of all values.
   */
  Array(Span<T> values) : Array(NoExceptConstructor())
  {

    data_ = this->get_buffer_for_size(values.size());
    uninitialized_copy_n(values.data(), values.size(), data_);
    size_ = values.size();
  }

  /**
   * Create a new array that contains copies of all values.
   */
  Array(const std::initializer_list<T> &values) : Array(Span<T>(values))
  {
  }

  /**
   * Create a new array with the given size. All values will be default constructed. For trivial
   * types like int, default construction does nothing.
   *
   * We might want another version of this in the future, that does not do default construction
   * even for non-trivial types. This should not be the default though, because one can easily mess
   * up when dealing with uninitialized memory.
   */
  explicit Array(uint size) : Array(NoExceptConstructor())
  {
    data_ = this->get_buffer_for_size(size);
    default_construct_n(data_, size);
    size_ = size;
  }

  /**
   * Create a new array with the given size. All values will be initialized by copying the given
   * default.
   */
  Array(uint size, const T &value) : Array(NoExceptConstructor())
  {
    data_ = this->get_buffer_for_size(size);
    uninitialized_fill_n(data_, size, value);
    size_ = size;
  }

  /**
   * Create a new array with uninitialized elements. The caller is responsible for constructing the
   * elements. Moving, copying or destructing an Array with uninitialized elements invokes
   * undefined behavior.
   *
   * This should be used very rarely. Note, that the normal size-constructor also does not
   * initialize the elements when T is trivially constructible. Therefore, it only makes sense to
   * use this with non trivially constructible types.
   *
   * Usage:
   *  Array<std::string> my_strings(10, NoInitialization());
   */
  Array(uint size, NoInitialization)
  {
    data_ = this->get_buffer_for_size(size);
    size_ = size;
  }

  Array(const Array &other) : Array(other.as_span())
  {
  }

  Array(Array &&other) noexcept : allocator_(other.allocator_)
  {
    size_ = other.size_;

    if (!other.uses_inline_buffer()) {
      data_ = other.data_;
    }
    else {
      data_ = this->get_buffer_for_size(size_);
      uninitialized_relocate_n(other.data_, size_, data_);
    }

    other.data_ = other.inline_buffer_;
    other.size_ = 0;
  }

  ~Array()
  {
    destruct_n(data_, size_);
    this->deallocate_if_not_inline(data_);
  }

  Array &operator=(const Array &other)
  {
    if (this == &other) {
      return *this;
    }

    Array copied_array{other};
    *this = std::move(copied_array);
    return *this;
  }

  Array &operator=(Array &&other) noexcept
  {
    if (this == &other) {
      return *this;
    }

    this->~Array();
    new (this) Array(std::move(other));
    return *this;
  }

  T &operator[](uint index) noexcept
  {
    BLI_assert(index < size_);
    return data_[index];
  }

  const T &operator[](uint index) const noexcept
  {
    BLI_assert(index < size_);
    return data_[index];
  }

  operator Span<T>() const noexcept
  {
    return Span<T>(data_, size_);
  }

  operator MutableSpan<T>() noexcept
  {
    return MutableSpan<T>(data_, size_);
  }

  Span<T> as_span() const noexcept
  {
    return *this;
  }

  MutableSpan<T> as_mutable_span() noexcept
  {
    return *this;
  }

  /**
   * Returns the number of elements in the array.
   */
  uint size() const noexcept
  {
    return size_;
  }

  /**
   * Returns true when the number of elements in the array is zero.
   */
  bool is_empty() const noexcept
  {
    return size_ == 0;
  }

  /**
   * Copies the value to all indices in the array.
   */
  void fill(const T &value)
  {
    initialized_fill_n(data_, size_, value);
  }

  /**
   * Copies the value to the given indices in the array.
   */
  void fill_indices(Span<uint> indices, const T &value)
  {
    MutableSpan<T>(*this).fill_indices(indices, value);
  }

  /**
   * Destruct values and create a new array of the given size. The values in the new array are
   * default constructed.
   */
  void reinitialize(const uint new_size)
  {
    uint old_size = size_;

    destruct_n(data_, size_);
    size_ = 0;

    if (new_size <= old_size) {
      default_construct_n(data_, new_size);
    }
    else {
      T *new_data = this->get_buffer_for_size(new_size);
      try {
        default_construct_n(new_data, new_size);
      }
      catch (...) {
        this->deallocate_if_not_inline(new_data);
        throw;
      }
      this->deallocate_if_not_inline(data_);
      data_ = new_data;
    }

    size_ = new_size;
  }

  /**
   * Get a pointer to the beginning of the array.
   */
  const T *data() const noexcept
  {
    return data_;
  }
  T *data() noexcept
  {
    return data_;
  }

  const T *begin() const noexcept
  {
    return data_;
  }

  const T *end() const noexcept
  {
    return data_ + size_;
  }

  T *begin() noexcept
  {
    return data_;
  }

  T *end() noexcept
  {
    return data_ + size_;
  }

  /**
   * Get an index range containing all valid indices for this array.
   */
  IndexRange index_range() const noexcept
  {
    return IndexRange(size_);
  }

  /**
   * Sets the size to zero. This should only be used when you have manually destructed all elements
   * in the array beforehand. Use with care.
   */
  void clear_without_destruct() noexcept
  {
    size_ = 0;
  }

  /**
   * Access the allocator used by this array.
   */
  Allocator &allocator() noexcept
  {
    return allocator_;
  }

  /**
   * Get the value of the InlineBufferCapacity template argument. This is the number of elements
   * that can be stored without doing an allocation.
   */
  static uint inline_buffer_capacity() noexcept
  {
    return InlineBufferCapacity;
  }

 private:
  T *get_buffer_for_size(uint size)
  {
    if (size <= InlineBufferCapacity) {
      return inline_buffer_;
    }
    else {
      return this->allocate(size);
    }
  }

  T *allocate(uint size)
  {
    return (T *)allocator_.allocate(size * sizeof(T), alignof(T), AT);
  }

  void deallocate_if_not_inline(T *ptr)
  {
    if (ptr != inline_buffer_) {
      allocator_.deallocate(ptr);
    }
  }

  bool uses_inline_buffer() const noexcept
  {
    return data_ == inline_buffer_;
  }
};

}  // namespace blender

#endif /* __BLI_ARRAY_HH__ */
