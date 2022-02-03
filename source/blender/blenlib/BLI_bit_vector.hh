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
#include "BLI_memory_utils.hh"
#include "BLI_span.hh"

namespace blender {

class BitRef {
 private:
  const uint8_t *byte_ptr_;
  uint8_t mask_;

 public:
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
  MutableBitRef(uint8_t *byte_ptr, const int64_t bit_index)
  {
    byte_ptr_ = byte_ptr + (bit_index >> 3);
    mask_ = 1 << static_cast<uint8_t>(bit_index & 7);
  }

  operator bool() const
  {
    const uint8_t byte = *byte_ptr_;
    const uint8_t masked_byte = byte & mask_;
    return masked_byte != 0;
  }

  MutableBitRef &operator=(const bool value)
  {
    const uint8_t old_byte = *byte_ptr_;
    const uint8_t cleared_byte = old_byte & ~mask_;
    const uint8_t new_byte = cleared_byte | (value & mask_);
    *byte_ptr_ = new_byte;
    return *this;
  }
};

template<int64_t InlineBufferCapacity = 16, typename Allocator = GuardedAllocator>
class BitVector {
 private:
  static constexpr int64_t BitsPerByte = 8;
  static constexpr int64_t BitsInInlineBuffer = InlineBufferCapacity * BitsPerByte;

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
      data_ = static_cast<uint8_t *>(allocator_.allocate(bytes_to_copy, 8, __func__));
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

 private:
  bool is_inline() const
  {
    return data_ == inline_buffer_;
  }

  int64_t used_bytes_amount() const
  {
    return (size_in_bits_ + BitsPerByte - 1) / BitsPerByte;
  }
};

}  // namespace blender
