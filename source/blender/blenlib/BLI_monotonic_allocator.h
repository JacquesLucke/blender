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

/** \file
 * \ingroup bli
 *
 * A monotonic allocator is the simplest form of an allocator. It never reuses any memory, and
 * therefore does not need a deallocation method. It simply hands out consecutive buffers of
 * memory. When the current buffer is full, it reallocates a new larger buffer and continues.
 */

#pragma once

#include "BLI_vector.h"
#include "BLI_utility_mixins.h"

namespace BLI {

template<typename Allocator = GuardedAllocator>
class MonotonicAllocator : NonCopyable, NonMovable {
 private:
  Allocator m_allocator;
  Vector<void *> m_pointers;

  void *m_current_buffer;
  uint m_remaining_capacity;
  uint m_next_min_alloc_size;

 public:
  MonotonicAllocator()
      : m_current_buffer(nullptr), m_remaining_capacity(0), m_next_min_alloc_size(16)
  {
  }

  ~MonotonicAllocator()
  {
    for (void *ptr : m_pointers) {
      m_allocator.deallocate(ptr);
    }
  }

  void *allocate(uint size)
  {
    if (size <= m_remaining_capacity) {
      void *ptr = m_current_buffer;
      m_remaining_capacity -= size;
      m_current_buffer = POINTER_OFFSET(ptr, size);
      return ptr;
    }
    else {
      uint byte_size = std::max({m_next_min_alloc_size, size, m_allocator.min_allocated_size()});

      void *ptr = m_allocator.allocate(byte_size, __func__);
      m_pointers.append(ptr);

      m_current_buffer = POINTER_OFFSET(ptr, size);
      m_next_min_alloc_size = byte_size * 2;
      m_remaining_capacity = byte_size - size;

      return ptr;
    }
  }

  template<typename T> T *allocate()
  {
    return (T *)this->allocate(sizeof(T));
  }

  template<typename T> MutableArrayRef<T> allocate_array(uint length)
  {
    return MutableArrayRef<T>((T *)this->allocate(sizeof(T) * length), length);
  }

  void *allocate_aligned(uint size, uint alignment)
  {
    /* TODO: Don't overallocate when not necessary. */
    BLI_assert(is_power_of_2_i(alignment));
    size_t space = size + alignment - 1;
    void *ptr = this->allocate(space);
    void *aligned_ptr = std::align(alignment, size, ptr, space);
    BLI_assert(aligned_ptr != nullptr);
    BLI_assert((POINTER_AS_UINT(aligned_ptr) & (alignment - 1)) == 0);
    return aligned_ptr;
  }
};

}  // namespace BLI
