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
 * A `BLI::Stack<T>` is a dynamically growing FILO (first-in, last-out) data structure. It is
 * designed to be a more convenient and efficient replacement for `std::stack`.
 *
 * The improved efficiency is mainly achieved by supporting small buffer optimization. As long as
 * the number of elements added to the stack stays below InlineBufferCapacity, no heap allocation
 * is done. Consequently, values stored in the stack have to be movable and they might be moved,
 * when the stack is moved.
 *
 * The implementation stores the elements in potentially multiple continuous chunks. The individual
 * chunks are connected by a double linked list. All, except the top-most chunk are always
 * completely full.
 */

#include "BLI_allocator.hh"
#include "BLI_array_ref.hh"
#include "BLI_memory_utils.hh"

namespace BLI {

/**
 * A StackChunk references a continuous memory buffer. Multiple StackChunk instances are linked in
 * a double linked list.
 *
 * The alignment of StackChunk is at least the alignment of T, because that makes it simpler to
 * allocate a StackChunk and the referenced memory in a single heap allocation.
 */
template<typename T> struct alignas(alignof(T)) StackChunk {
  /** The below chunk contains the elements that have been pushed on the stack before. */
  StackChunk *below;
  /** The above chunk contains the elements that have been pushed on the stack afterwards. */
  StackChunk *above;
  /** Pointer to the first element of the referenced buffer. */
  T *begin;
  /** Pointer to one element past the end of the referenced buffer. */
  T *capacity_end;

  uint capacity() const
  {
    return capacity_end - begin;
  }
};

template<
    /** Type of the elements that are stored in the stack. */
    typename T,
    /**
     * The number of values that can be stored in this stack, without doing a heap allocation.
     * Sometimes it can make sense to increase this value a lot. The memory in the inline buffer is
     * not initialized when it is not needed.
     *
     * When T is large, the small buffer optimization is disabled by default to avoid large
     * unexpected allocations on the stack. It can still be enabled explicitely though.
     */
    uint InlineBufferCapacity = (sizeof(T) < 100) ? 4 : 0,
    /**
     * The allocator used by this stack. Should rarely be changed, except when you don't want that
     * MEM_mallocN etc. is used internally.
     */
    typename Allocator = GuardedAllocator>
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

  Stack(const std::initializer_list<T> &values) : Stack(ArrayRef<T>(values))
  {
  }

  Stack(const Stack &other) : Stack()
  {
    for (const Chunk *chunk = &other.m_inline_chunk; chunk; chunk = chunk->above) {
      const T *begin = chunk->begin;
      const T *end = (chunk == other.m_top_chunk) ? other.m_top : chunk->capacity_end;
      this->push_multiple(ArrayRef<T>(begin, end - begin));
    }
  }

  Stack(Stack &&other) : Stack()
  {
    uninitialized_relocate_n(other.inline_buffer(),
                             std::min(other.m_size, InlineBufferCapacity),
                             this->inline_buffer());

    m_inline_chunk.above = other.m_inline_chunk.above;
    m_size = other.m_size;

    if (m_size <= InlineBufferCapacity) {
      m_top_chunk = &m_inline_chunk;
      m_top = this->inline_buffer() + m_size;
    }
    else {
      m_top_chunk = other.m_top_chunk;
      m_top = other.m_top;
    }

    other.m_size = 0;
    other.m_inline_chunk.above = nullptr;
    other.m_top_chunk = &other.m_inline_chunk;
    other.m_top = other.m_top_chunk->begin;
  }

  ~Stack()
  {
    this->destruct_all_elements();
    Chunk *above_chunk;
    for (Chunk *chunk = m_inline_chunk.above; chunk; chunk = above_chunk) {
      above_chunk = chunk->above;
      m_allocator.deallocate(chunk);
    }
  }

  Stack &operator=(const Stack &stack)
  {
    if (this == &stack) {
      return *this;
    }

    this->~Stack();
    new (this) Stack(stack);

    return *this;
  }

  Stack &operator=(Stack &&stack)
  {
    if (this == &stack) {
      return *this;
    }

    this->~Stack();
    new (this) Stack(std::move(stack));

    return *this;
  }

  void push(const T &value)
  {
    if (m_top == m_top_chunk->capacity_end) {
      this->grow(1);
    }
    new (m_top) T(value);
    m_top++;
    m_size++;
  }

  void push(T &&value)
  {
    if (m_top == m_top_chunk->capacity_end) {
      this->grow(1);
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
    uint remaining_capacity = this->remaining_capacity_in_top_chunk();
    uint amount = std::min(values.size(), remaining_capacity);
    uninitialized_copy_n(values.begin(), amount, m_top);
    m_top += amount;

    ArrayRef<T> remaining_values = values.drop_front(amount);
    uint remaining_amount = remaining_values.size();
    if (remaining_amount > 0) {
      this->grow(remaining_amount);
      uninitialized_copy_n(remaining_values.begin(), remaining_amount, m_top);
      m_top += remaining_amount;
    }

    m_size += values.size();
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

  void grow(uint min_new_elements)
  {
    if (m_top_chunk->above == nullptr) {
      uint new_capacity = std::max(min_new_elements, m_top_chunk->capacity() * 2 + 10);

      BLI_STATIC_ASSERT(sizeof(Chunk) % alignof(T) == 0, "");
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

  uint remaining_capacity_in_top_chunk() const
  {
    return m_top_chunk->capacity_end - m_top;
  }
};

} /* namespace BLI */

#endif /* __BLI_STACK_HH__ */
