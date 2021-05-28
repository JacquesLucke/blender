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

#include "BLI_allocator.hh"
#include "BLI_memory_utils.hh"
#include "BLI_span.hh"

namespace blender {

template<typename T> struct QueueChunk {
  /**
   * Pointer to the chunk that should be used once this chunk is empty.
   * Null when this is the last chunk.
   */
  QueueChunk *next;

  /**
   * Bounds of the memory buffer corresponding to this chunk.
   */
  T *capacity_begin;
  T *capacity_end;

  /**
   * Points to the place where the next element should be added.
   */
  T *top_;
};

template<typename T,
         int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(T)),
         typename Allocator = GuardedAllocator>
class Queue {
 public:
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using size_type = int64_t;

 private:
  using Chunk = QueueChunk<T>;

  Chunk *pop_chunk_;
  Chunk *push_chunk_;
  T *bottom_;

  int64_t size_;

  TypedBuffer<T, InlineBufferCapacity> inline_buffer_;
  Chunk inline_chunk_;

  Allocator allocator_;

 public:
  Queue(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
    inline_chunk_.next = nullptr;
    inline_chunk_.capacity_begin = inline_buffer_;
    inline_chunk_.capacity_end = inline_buffer_ + InlineBufferCapacity;
    inline_chunk_.top_ = inline_buffer_;

    pop_chunk_ = &inline_chunk_;
    push_chunk_ = &inline_chunk_;
    bottom_ = inline_buffer_;
    size_ = 0;
  }

  Queue(NoExceptConstructor, Allocator allocator = {}) noexcept : Queue(allocator)
  {
  }

  ~Queue()
  {
    if (push_chunk_ == pop_chunk_) {
      if (next_pop_ <= next_push_) {
        this->destruct_between(next_pop_, next_push_);
      }
      else {
        this->destruct_between(next_pop_, current_chunk_->capacity_end);
        this->destruct_between(current_chunk_->capacity_begin, next_push_);
      }
    }
  }

  void push(const T &value)
  {
    this->push_as(value);
  }

  void push(T &&value)
  {
    this->push_as(std::move(value));
  }

  template<typename... Args> void push_as(Args &&... args)
  {
    this->ensure_space_for_one();
    new (next_push_) T(std::forward<Args>(args)...);
    size_++;
    next_push_++;
    i
  }

  T pop();

  int64_t size() const;

  bool is_empty() const
  {
    return this->size() == 0;
  }

 private:
  void destruct_between(T *begin, T *end)
  {
    BLI_assert(begin <= end);
    destruct_n(begin, end - begin);
  }
};

}  // namespace blender
