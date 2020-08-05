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
     */
    int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(T)),
    /**
     * The allocator used by this array. Should rarely be changed, except when you don't want that
     * MEM_* functions are used internally.
     */
    typename Allocator = GuardedAllocator>
class Array {
 private:
  /** The beginning of the array. It might point into the inline buffer. */
  T *data_;

  CompressedTriple<int64_t, Allocator, TypedBuffer<T, InlineBufferCapacity>>
      size_and_allocator_and_inline_buffer_;

 public:
  /**
   * By default an empty array is created.
   */
  Array()
  {
    data_ = this->inline_buffer();
    this->size_ref() = 0;
  }

  /**
   * Create a new array that contains copies of all values.
   */
  template<typename U, typename std::enable_if_t<std::is_convertible_v<U, T>> * = nullptr>
  Array(Span<U> values, Allocator allocator = {})
      : size_and_allocator_and_inline_buffer_(0, allocator, {})
  {
    this->size_ref() = values.size();
    data_ = this->get_buffer_for_size(values.size());
    uninitialized_convert_n<U, T>(values.data(), this->size(), data_);
  }

  /**
   * Create a new array that contains copies of all values.
   */
  template<typename U, typename std::enable_if_t<std::is_convertible_v<U, T>> * = nullptr>
  Array(const std::initializer_list<U> &values) : Array(Span<U>(values))
  {
  }

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
  explicit Array(int64_t size)
  {
    this->size_ref() = size;
    data_ = this->get_buffer_for_size(size);
    default_construct_n(data_, size);
  }

  /**
   * Create a new array with the given size. All values will be initialized by copying the given
   * default.
   */
  Array(int64_t size, const T &value)
  {
    BLI_assert(size >= 0);
    this->size_ref() = size;
    data_ = this->get_buffer_for_size(size);
    uninitialized_fill_n(data_, this->size(), value);
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
  Array(int64_t size, NoInitialization)
  {
    BLI_assert(size >= 0);
    this->size_ref() = size;
    data_ = this->get_buffer_for_size(size);
  }

  Array(const Array &other) : Array(other.as_span(), other.allocator())
  {
  }

  Array(Array &&other) noexcept : size_and_allocator_and_inline_buffer_(0, other.allocator(), {})
  {
    this->size_ref() = other.size();

    if (!other.uses_inline_buffer()) {
      data_ = other.data_;
    }
    else {
      data_ = this->get_buffer_for_size(this->size());
      uninitialized_relocate_n(other.data_, this->size(), data_);
    }

    other.data_ = other.inline_buffer();
    other.size_ref() = 0;
  }

  ~Array()
  {
    destruct_n(data_, this->size());
    if (!this->uses_inline_buffer()) {
      this->allocator().deallocate((void *)data_);
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

  T &operator[](int64_t index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->size());
    return data_[index];
  }

  const T &operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->size());
    return data_[index];
  }

  operator Span<T>() const
  {
    return Span<T>(data_, this->size());
  }

  operator MutableSpan<T>()
  {
    return MutableSpan<T>(data_, this->size());
  }

  template<typename U, typename std::enable_if_t<is_convertible_pointer_v<T, U>> * = nullptr>
  operator Span<U>() const
  {
    return Span<U>(data_, this->size());
  }

  template<typename U, typename std::enable_if_t<is_convertible_pointer_v<T, U>> * = nullptr>
  operator MutableSpan<U>()
  {
    return MutableSpan<U>(data_, this->size());
  }

  Span<T> as_span() const
  {
    return *this;
  }

  MutableSpan<T> as_mutable_span()
  {
    return *this;
  }

  /**
   * Returns the number of elements in the array.
   */
  int64_t size() const
  {
    return size_and_allocator_and_inline_buffer_.first();
  }

  /**
   * Returns true when the number of elements in the array is zero.
   */
  bool is_empty() const
  {
    return this->size() == 0;
  }

  /**
   * Copies the given value to every element in the array.
   */
  void fill(const T &value) const
  {
    initialized_fill_n(data_, this->size(), value);
  }

  /**
   * Get a pointer to the beginning of the array.
   */
  const T *data() const
  {
    return data_;
  }
  T *data()
  {
    return data_;
  }

  const T *begin() const
  {
    return data_;
  }

  const T *end() const
  {
    return data_ + this->size();
  }

  T *begin()
  {
    return data_;
  }

  T *end()
  {
    return data_ + this->size();
  }

  /**
   * Get an index range containing all valid indices for this array.
   */
  IndexRange index_range() const
  {
    return IndexRange(this->size());
  }

  /**
   * Sets the size to zero. This should only be used when you have manually destructed all elements
   * in the array beforehand. Use with care.
   */
  void clear_without_destruct()
  {
    this->size_ref() = 0;
  }

  /**
   * Access the allocator used by this array.
   */
  Allocator &allocator()
  {
    return size_and_allocator_and_inline_buffer_.second();
  }

  const Allocator &allocator() const
  {
    return size_and_allocator_and_inline_buffer_.second();
  }

  /**
   * Get the value of the InlineBufferCapacity template argument. This is the number of elements
   * that can be stored without doing an allocation.
   */
  static int64_t inline_buffer_capacity()
  {
    return InlineBufferCapacity;
  }

 private:
  T *get_buffer_for_size(int64_t size)
  {
    if (size <= InlineBufferCapacity) {
      return this->inline_buffer();
    }
    else {
      return this->allocate(size);
    }
  }

  T *allocate(int64_t size)
  {
    return (T *)this->allocator().allocate((size_t)size * sizeof(T), alignof(T), AT);
  }

  bool uses_inline_buffer() const
  {
    return data_ == this->inline_buffer();
  }

  TypedBuffer<T, InlineBufferCapacity> &inline_buffer()
  {
    return size_and_allocator_and_inline_buffer_.third();
  }

  const TypedBuffer<T, InlineBufferCapacity> &inline_buffer() const
  {
    return size_and_allocator_and_inline_buffer_.third();
  }

  int64_t &size_ref()
  {
    return size_and_allocator_and_inline_buffer_.first();
  }
};

/**
 * Same as a normal Array, but does not use Blender's guarded allocator. This is useful when
 * allocating memory with static storage duration.
 */
template<typename T, int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(T))>
using RawArray = Array<T, InlineBufferCapacity, RawAllocator>;

}  // namespace blender

#endif /* __BLI_ARRAY_HH__ */
