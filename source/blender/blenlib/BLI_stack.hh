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

#ifndef __BLI_STACK_HH__
#define __BLI_STACK_HH__

/** \file
 * \ingroup bli
 *
 * Basic stack implementation with support for small object optimization.
 */

#include "BLI_vector.hh"

namespace BLI {

template<typename T> struct alignas(alignof(T)) StackChunk {
  StackChunk *below;
  StackChunk *above;
  T *begin;
  T *capacity_end;

  uint capacity() const
  {
    return capacity_end - begin;
  }
};

template<typename T, uint InlineBufferCapacity = 4, typename Allocator = GuardedAllocator>
class Stack {
 private:
  using Chunk = StackChunk<T>;

  T *m_top;
  Chunk *m_top_chunk;
  uint m_size;

  AlignedBuffer<sizeof(T) * InlineBufferCapacity, alignof(T)> m_inline_buffer;
  Chunk m_inline_chunk;

  Allocator m_allocator;

 public:
  Stack()
  {
    T *inline_buffer = this->inline_buffer();

    m_inline_chunk.below = nullptr;
    m_inline_chunk.above = nullptr;
    m_inline_chunk.begin = inline_buffer;
    m_inline_chunk.capacity_end = inline_buffer + InlineBufferCapacity;

    m_top = inline_buffer;
    m_top_chunk = &m_inline_chunk;
    m_size = 0;
  }

  Stack(ArrayRef<T> values) : Stack()
  {
    this->push_multiple(values);
  }

  ~Stack()
  {
    this->destruct_all_elements();
    for (Chunk *chunk = m_top_chunk; chunk != &m_inline_chunk; chunk = chunk->below) {
      m_allocator.deallocate(chunk);
    }
  }

  void push(const T &value)
  {
    if (m_top == m_top_chunk->capacity_end) {
      this->grow();
    }
    new (m_top) T(value);
    m_top++;
    m_size++;
  }

  void push(T &&value)
  {
    if (m_top == m_top_chunk->capacity_end) {
      this->grow();
    }
    new (m_top) T(std::move(value));
    m_top++;
    m_size++;
  }

  T pop()
  {
    BLI_assert(m_size > 0);
    m_top--;
    T value = std::move(*m_top);
    m_top->~T();
    m_size--;

    if (m_top == m_top_chunk->begin) {
      if (m_top_chunk->below != nullptr) {
        m_top_chunk = m_top_chunk->below;
        m_top = m_top_chunk->capacity_end;
      }
    }
    return value;
  }

  T &peek()
  {
    BLI_assert(m_size > 0);
    BLI_assert(m_top > m_top_chunk->begin);
    return *(m_top - 1);
  }

  const T &peek() const
  {
    BLI_assert(m_size > 0);
    BLI_assert(m_top > m_top_chunk->begin);
    return *(m_top - 1);
  }

  void push_multiple(ArrayRef<T> values)
  {
    for (const T &value : values) {
      this->push(value);
    }
  }

  bool is_empty() const
  {
    return m_size == 0;
  }

  uint size() const
  {
    return m_size;
  }

  void clear()
  {
    this->destruct_all_elements();
    m_top_chunk = &m_inline_chunk;
    m_top = m_top_chunk->begin;
  }

 private:
  T *inline_buffer() const
  {
    return (T *)m_inline_buffer.ptr();
  }

  void grow()
  {
    if (m_top_chunk->above == nullptr) {
      uint new_capacity = m_top_chunk->capacity() * 2 + 10;
      void *buffer = m_allocator.allocate_aligned(
          sizeof(Chunk) + sizeof(T) * new_capacity, alignof(Chunk), "grow stack");
      Chunk *new_chunk = new (buffer) Chunk();
      new_chunk->begin = (T *)POINTER_OFFSET(buffer, sizeof(Chunk));
      new_chunk->capacity_end = new_chunk->begin + new_capacity;
      new_chunk->above = nullptr;
      new_chunk->below = m_top_chunk;
      m_top_chunk->above = new_chunk;
    }
    m_top_chunk = m_top_chunk->above;
    m_top = m_top_chunk->begin;
  }

  void destruct_all_elements()
  {
    for (T *value = m_top_chunk->begin; value != m_top; value++) {
      value->~T();
    }
    for (Chunk *chunk = m_top_chunk->below; chunk; chunk = chunk->below) {
      for (T *value = chunk->begin; value != chunk->capacity_end; value++) {
        value->~T();
      }
    }
  }
};

} /* namespace BLI */

#endif /* __BLI_STACK_HH__ */
