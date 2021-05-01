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
 *
 * A `SingleProducerChunkConsumerQueue<T>` is designed to handle the case when
 * - A single producer thread wants to append elements to the queue very efficiently.
 * - A single consumer thread wants to consume large chunks from the queue at a time.
 * - The producer and consumer might run on different threads.
 */

#include <atomic>
#include <mutex>

#include "BLI_allocator.hh"
#include "BLI_function_ref.hh"
#include "BLI_span.hh"
#include "BLI_utility_mixins.hh"

namespace blender {

template<typename T, typename Allocator = GuardedAllocator>
class SingleProducerChunkConsumerQueue {
 private:
  struct Chunk {
    /**
     * Points to the next chunk that contains the elements added after the elements in this chunk.
     * This is only modified during the append-operation. When it is not null, it means that the
     * append-operation will not look at this chunk anymore.
     */
    std::atomic<Chunk *> next = nullptr;

    /**
     * Number of elements that have been committed to the chunk and won't be modified anymore.
     * This is modified during the append-operation and is only increasing.
     */
    std::atomic<int64_t> committed_size = 0;

    /**
     * Number of elements that have been consumed already from this chunk.
     * This is only accessed by the consume-operation.
     */
    int64_t consumed_size = 0;

    /**
     * Begin and end of the entire chunk buffer. Those are only set during construction and don't
     * change anymore afterwards.
     */
    T *begin = nullptr;
    T *capacity_end = nullptr;

    /**
     * This is modified by the append-operation and not accessed by the consume-operation.
     */
    T *end = nullptr;
  };

  static constexpr inline int64_t ChunkCapacity = 1000;

  Allocator allocator_;

  /* Is only modified in constructor and during consume. */
  Chunk *begin_;

  /* Is only accessed when appending. */
  Chunk *current_;

 public:
  SingleProducerChunkConsumerQueue()
  {
    /* Create the first chunk in the constructor, so that the append-operation does not have to
     * handle this case. */
    begin_ = this->new_chunk();
    current_ = begin_;
  }

  ~SingleProducerChunkConsumerQueue()
  {
    Chunk *chunk = begin_;
    while (chunk) {
      Chunk *next_chunk = chunk->next;
      this->delete_chunk(chunk);
      chunk = next_chunk;
    }
  }

  /**
   * Start appending a new element.
   * This constructs the new element with the given parameters.
   * This must be used in conjunction with #commit_append.
   */
  template<typename... Args> T *prepare_append(Args &&... args)
  {
    if (current_->end == current_->capacity_end) {
      /* Create a new chunk when the current one is full. */
      Chunk *new_chunk = this->new_chunk();
      /* This tells the consume-operation that the append-operation does not look at this chunk
       * anymore. */
      current_->next.store(new_chunk, std::memory_order_release);
      current_ = new_chunk;
    }
    /* Return a pointer to the next element. */
    return new (current_->end) T(std::forward<Args>(args)...);
  }

  /**
   * Tell the queue that the element has been constructed and is ready to be committed.
   * Once it is committed, the next consumer can read it.
   */
  void commit_append()
  {
    current_->end++;
    /* Compute the committed size like that instead of doing an increment to avoid having a
     * read-modify-write operation on an atomic variable which could be more expensive than just
     * writing to it. */
    const int64_t new_committed_size = current_->end - current_->begin;
    current_->committed_size.store(new_committed_size, std::memory_order_release);
  }

  /**
   * Get access to all newly committed elements in this queue.
   * Only a single thread is allowed to entire this function.
   * However, another thread is allowed to perform an append-operation at the same time.
   * The spans passed to `consume_fn` are valid until `free_consumed` has been called.
   */
  void consume(const FunctionRef<void(Span<T>)> consume_fn)
  {
    Chunk *chunk = begin_;
    while (chunk) {
      const int64_t already_consumed_size = chunk->consumed_size;
      const int64_t committed_chunk_size = chunk->committed_size.load(std::memory_order_acquire);
      const int64_t newly_consumed_size = committed_chunk_size - already_consumed_size;

      const Span<T> committed_data{chunk->begin + already_consumed_size, newly_consumed_size};
      consume_fn(committed_data);
      chunk->consumed_size = committed_chunk_size;

      /* Only try to consume the next chunk, if all elements from this chunk have been consumed. */
      if (committed_chunk_size == ChunkCapacity) {
        chunk = chunk->next.load(std::memory_order_acquire);
      }
      else {
        break;
      }
    }
  }

  /**
   * Free chunks that have been consumed already and won't be accessed anymore.
   * Calling this will invalidate the spans provided by #consume.
   */
  void free_consumed()
  {
    Chunk *chunk = begin_;
    while (chunk) {
      const int64_t consumed_size = chunk->consumed_size;
      /* Check if the entire capacity of the chunk has been consumed. */
      if (consumed_size == ChunkCapacity) {
        /* Check if the append-operation might still access this chunk. */
        Chunk *next_chunk = chunk->next.load(std::memory_order_acquire);
        if (next_chunk != nullptr) {
          begin_ = next_chunk;
          this->delete_chunk(chunk);
        }
        chunk = next_chunk;
      }
      else {
        break;
      }
    }
  }

 private:
  Chunk *new_chunk()
  {
    /* We could also combine both allocations in a single one. */
    void *chunk_buffer = allocator_.allocate(sizeof(Chunk), alignof(Chunk), __func__);
    Chunk *chunk = new (chunk_buffer) Chunk();
    chunk->begin = (T *)allocator_.allocate(
        sizeof(T) * (size_t)ChunkCapacity, alignof(T), __func__);
    chunk->end = chunk->begin;
    chunk->capacity_end = chunk->begin + ChunkCapacity;
    return chunk;
  }

  void delete_chunk(Chunk *chunk)
  {
    destruct_n(chunk->begin, chunk->committed_size);
    allocator_.deallocate(chunk->begin);
    chunk->~Chunk();
    allocator_.deallocate(chunk);
  }
};

}  // namespace blender
