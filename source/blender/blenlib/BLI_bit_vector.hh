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

#pragma once

/** \file
 * \ingroup bli
 */

#include <cstring>

#include "BLI_allocator.hh"
#include "BLI_index_range.hh"
#include "BLI_memory_utils.hh"
#include "BLI_span.hh"

namespace blender {

class MutableBitRef;

class BitRef {
 private:
  const uint8_t *byte_ptr_;
  uint8_t mask_;

  friend MutableBitRef;

 public:
  BitRef() = default;

  BitRef(const uint8_t *byte_ptr, const int64_t bit_index)
  {
    byte_ptr_ = byte_ptr + (bit_index >> 3);
    mask_ = 1 << (bit_index & 7);
  }

  operator bool() const
  {
    const uint8_t byte = *byte_ptr_;
    const uint8_t masked_byte = byte & mask_;
    return masked_byte != 0;
  }
};

class MutableBitRef {
 private:
  uint8_t *byte_ptr_;
  uint8_t mask_;

 public:
  MutableBitRef() = default;

  MutableBitRef(uint8_t *byte_ptr, const int64_t bit_index)
  {
    byte_ptr_ = byte_ptr + (bit_index >> 3);
    mask_ = 1 << static_cast<uint8_t>(bit_index & 7);
  }

  operator BitRef() const
  {
    BitRef bit_ref;
    bit_ref.byte_ptr_ = byte_ptr_;
    bit_ref.mask_ = mask_;
    return bit_ref;
  }

  operator bool() const
  {
    const uint8_t byte = *byte_ptr_;
    const uint8_t masked_byte = byte & mask_;
    return masked_byte != 0;
  }

  void enable()
  {
    *byte_ptr_ |= mask_;
  }

  void disable()
  {
    *byte_ptr_ &= ~mask_;
  }

  void set(const bool value)
  {
    if (value) {
      this->enable();
    }
    else {
      this->disable();
    }
  }
};

template<int64_t InlineBufferCapacity = 4, typename Allocator = GuardedAllocator> class BitVector {
 private:
  static constexpr int64_t BitsPerByte = 8;
  static constexpr int64_t BitsInInlineBuffer = InlineBufferCapacity * BitsPerByte;
  static constexpr int64_t AllocationAlignment = 8;

  uint8_t *data_;
  int64_t size_in_bits_;
  int64_t capacity_in_bits_;

  Allocator allocator_;
  TypedBuffer<uint8_t, InlineBufferCapacity> inline_buffer_;

 public:
  BitVector(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
    data_ = inline_buffer_;
    size_in_bits_ = 0;
    capacity_in_bits_ = BitsInInlineBuffer;
  }

  BitVector(NoExceptConstructor, Allocator allocator = {}) noexcept : BitVector(allocator)
  {
  }

  BitVector(const BitVector &other) : BitVector(NoExceptConstructor(), other.allocator_)
  {
    const int64_t bytes_to_copy = other.used_bytes_amount();
    if (other.size_in_bits_ <= BitsInInlineBuffer) {
      data_ = inline_buffer_;
      capacity_in_bits_ = BitsInInlineBuffer;
    }
    else {
      data_ = static_cast<uint8_t *>(
          allocator_.allocate(bytes_to_copy, AllocationAlignment, __func__));
      capacity_in_bits_ = bytes_to_copy * BitsPerByte;
    }
    size_in_bits_ = other.size_in_bits_;
    uninitialized_copy_n(other.data_, bytes_to_copy, data_);
  }

  BitVector(BitVector &&other) noexcept : BitVector(NoExceptConstructor(), other.allocator_)
  {
    if (other.is_inline()) {
      const int64_t bytes_to_copy = other.used_bytes_amount();
      data_ = inline_buffer_;
      uninitialized_copy_n(other.data_, bytes_to_copy, data_);
    }
    else {
      /* Steal the pointer. */
      data_ = other.data_;
    }
    size_in_bits_ = other.size_in_bits_;
    capacity_in_bits_ = other.capacity_in_bits_;

    other.data_ = other.inline_buffer_;
    other.size_in_bits_ = 0;
    other.capacity_in_bits_ = BitsInInlineBuffer;
  }

  explicit BitVector(const int64_t size_in_bits, Allocator allocator = {})
      : BitVector(NoExceptConstructor(), allocator)
  {
    this->resize(size_in_bits);
  }

  BitVector(const int64_t size_in_bits, const bool value, Allocator allocator = {})
      : BitVector(NoExceptConstructor(), allocator)
  {
    this->resize(size_in_bits, value);
  }

  explicit BitVector(const Span<bool> values, Allocator allocator = {})
      : BitVector(NoExceptConstructor(), allocator)
  {
    this->resize(values.size());
    for (const int64_t i : this->index_range()) {
      (*this)[i].set(values[i]);
    }
  }

  ~BitVector()
  {
    if (!this->is_inline()) {
      allocator_.deallocate(data_);
    }
  }

  BitVector &operator=(const BitVector &other)
  {
    return copy_assign_container(*this, other);
  }

  BitVector &operator=(BitVector &&other)
  {
    return move_assign_container(*this, std::move(other));
  }

  int64_t size() const
  {
    return size_in_bits_;
  }

  BitRef operator[](const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_in_bits_);
    return {data_, index};
  }

  MutableBitRef operator[](const int64_t index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_in_bits_);
    return {data_, index};
  }

  IndexRange index_range() const
  {
    return {0, size_in_bits_};
  }

  void append(const bool value)
  {
    this->ensure_space_for_one();
    MutableBitRef bit{data_, size_in_bits_};
    bit.set(value);
    size_in_bits_++;
  }

  class Iterator {
   private:
    const BitVector *vector_;
    int64_t index_;

   public:
    Iterator(const BitVector &vector, const int64_t index) : vector_(&vector), index_(index)
    {
    }

    Iterator &operator++()
    {
      index_++;
      return *this;
    }

    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      BLI_assert(a.vector_ == b.vector_);
      return a.index_ != b.index_;
    }

    BitRef operator*() const
    {
      return (*vector_)[index_];
    }
  };

  class MutableIterator {
   private:
    BitVector *vector_;
    int64_t index_;

   public:
    MutableIterator(BitVector &vector, const int64_t index) : vector_(&vector), index_(index)
    {
    }

    MutableIterator &operator++()
    {
      index_++;
      return *this;
    }

    friend bool operator!=(const MutableIterator &a, const MutableIterator &b)
    {
      BLI_assert(a.vector_ == b.vector_);
      return a.index_ != b.index_;
    }

    MutableBitRef operator*() const
    {
      return (*vector_)[index_];
    }
  };

  Iterator begin() const
  {
    return {*this, 0};
  }

  Iterator end() const
  {
    return {*this, size_in_bits_};
  }

  MutableIterator begin()
  {
    return {*this, 0};
  }

  MutableIterator end()
  {
    return {*this, size_in_bits_};
  }

  void resize(const int64_t new_size_in_bits)
  {
    BLI_assert(new_size_in_bits >= 0);
    if (new_size_in_bits > size_in_bits_) {
      this->reserve(new_size_in_bits);
    }
    size_in_bits_ = new_size_in_bits;
  }

  void resize(const int64_t new_size_in_bits, const bool value)
  {
    const int64_t old_size_in_bits = size_in_bits_;
    this->resize(new_size_in_bits);
    if (old_size_in_bits < new_size_in_bits) {
      this->fill_range(IndexRange(old_size_in_bits, new_size_in_bits - old_size_in_bits), value);
    }
  }

  void fill_range(const IndexRange range, const bool value)
  {
    /* Can be optimized by filling entire bytes at once. */
    for (const int64_t i : range) {
      (*this)[i].set(value);
    }
  }

  void reserve(const int new_capacity_in_bits)
  {
    this->realloc_to_at_least(new_capacity_in_bits);
  }

 private:
  void ensure_space_for_one()
  {
    if (UNLIKELY(size_in_bits_ >= capacity_in_bits_)) {
      this->realloc_to_at_least(size_in_bits_ + 1);
    }
  }

  BLI_NOINLINE void realloc_to_at_least(const int64_t min_capacity_in_bits)
  {
    if (capacity_in_bits_ >= min_capacity_in_bits) {
      return;
    }

    const int64_t min_capacity_in_bytes = this->required_bytes_for_bits(min_capacity_in_bits);

    /* At least double the size of the previous allocation. */
    const int64_t min_new_capacity_in_bytes = capacity_in_bits_ * 2;

    const int64_t new_capacity_in_bytes = std::max(min_capacity_in_bytes,
                                                   min_new_capacity_in_bytes);
    const int64_t bytes_to_copy = this->used_bytes_amount();

    uint8_t *new_data = static_cast<uint8_t *>(
        allocator_.allocate(new_capacity_in_bytes, AllocationAlignment, __func__));
    uninitialized_copy_n(data_, bytes_to_copy, new_data);

    if (!this->is_inline()) {
      allocator_.deallocate(data_);
    }

    data_ = new_data;
    capacity_in_bits_ = new_capacity_in_bytes * BitsPerByte;
  }

  bool is_inline() const
  {
    return data_ == inline_buffer_;
  }

  int64_t used_bytes_amount() const
  {
    return this->required_bytes_for_bits(size_in_bits_);
  }

  static int64_t required_bytes_for_bits(const int64_t number_of_bits)
  {
    return (number_of_bits + BitsPerByte - 1) / BitsPerByte;
  }
};

}  // namespace blender
