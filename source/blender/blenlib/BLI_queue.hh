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
  QueueChunk *next_chunk;

  /**
   * Bounds of the memory buffer corresponding to this chunk.
   */
  T *capacity_begin;
  T *capacity_end;

  /**
   * Points to the place where the next element should be added.
   */
  T *next_push;
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
  T *next_pop_;

  T **pop_span_end_;
  T **push_span_end_;

  int64_t size_;

  TypedBuffer<T, InlineBufferCapacity> inline_buffer_;
  Chunk inline_chunk_;

  Allocator allocator_;

 public:
  Queue(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
    inline_chunk_.next_chunk = nullptr;
    inline_chunk_.capacity_begin = inline_buffer_;
    inline_chunk_.capacity_end = inline_buffer_ + InlineBufferCapacity;
    inline_chunk_.top_ = inline_buffer_;

    pop_chunk_ = &inline_chunk_;
    push_chunk_ = &inline_chunk_;
    next_pop_ = inline_buffer_;
    size_ = 0;

    pop_span_end_ = next_pop_;
    push_span_end_ = inline_chunk_.capacity_end;
  }

  Queue(NoExceptConstructor, Allocator allocator = {}) noexcept : Queue(allocator)
  {
  }

  ~Queue()
  {
    /* The first chunk might be used as ring buffer, so we might need to destruct two subspans. */
    if (push_chunk_ == pop_chunk_) {
      Chunk *chunk = pop_chunk_;
      if (next_pop_ <= chunk->next) {
        this->destruct_between(next_pop_, chunk->next_push_);
      }
      else {
        this->destruct_between(next_pop_, chunk->capacity_end);
        this->destruct_between(chunk->capacity_begin, chunk->next_push);
      }
    }

    /* The other chunks have only been pushed to yet, so they are single spans. */
    Chunk *chunk = pop_chunk_->next_chunk;
    while (chunk != nullptr) {
      this->destruct_between(chunk->capacity_begin, chunk->next_push);
    }

    /* Free chunks. */
    chunk = pop_chunk_;
    while (chunk != nullptr) {
      Chunk *next_chunk = chunk->next_chunk;
      allocator_.deallocate(next_chunk->capacity_begin);
      allocator_.deallocate(next_chunk);
      chunk = next_chunk;
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
    this->ensure_can_push_one();
    new (push_chunk_->next_push) T(std::forward<Args>(args)...);
    push_chunk_->next_push++;
    size_++;
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

  void ensure_can_push_one()
  {
    if (LIKELY(push_chunk_->next_push < *push_span_end_)) {
      return;
    }
    if (push_chunk_ != pop_chunk_) {
      if (push_chunk_->next_push < push_chunk_->capacity_end) {
        return;
      }
    }
    else {
      Chunk *chunk = push_chunk_;
      if (chunk->next_push < next_pop_) {
        return;
      }
      if (chunk->next_push < chunk->capacity_end) {
        return;
      }
    }
    /* TODO: Add new chunk. */
  }
};

}  // namespace blender
