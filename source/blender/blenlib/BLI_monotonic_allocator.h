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
#include "BLI_timeit.h"
#include "BLI_string_ref.h"

namespace BLI {

template<uint N = 0, typename Allocator = GuardedAllocator>
class MonotonicAllocator : NonCopyable, NonMovable {
 private:
  Allocator m_allocator;
  Vector<void *> m_pointers;

  void *m_current_buffer;
  uint m_remaining_capacity;
  uint m_next_min_alloc_size;

  AlignedBuffer<N, 8> m_inline_buffer;

 public:
  MonotonicAllocator() : m_remaining_capacity(N), m_next_min_alloc_size(std::max<uint>(N * 2, 16))
  {
    m_current_buffer = m_inline_buffer.ptr();
  }

  ~MonotonicAllocator()
  {
    for (void *ptr : m_pointers) {
      m_allocator.deallocate(ptr);
    }
  }

  template<typename T> T *allocate()
  {
    return (T *)this->allocate(sizeof(T), alignof(T));
  }

  template<typename T> MutableArrayRef<T> allocate_array(uint length)
  {
    return MutableArrayRef<T>((T *)this->allocate(sizeof(T) * length), length);
  }

  void *allocate(uint size, uint alignment = 4)
  {
    BLI_assert(alignment >= 1);
    BLI_assert(is_power_of_2_i(alignment));

    uintptr_t alignment_mask = alignment - 1;

    uintptr_t current_buffer = (uintptr_t)m_current_buffer;
    uintptr_t potential_allocation_begin = (current_buffer + alignment - 1) & ~alignment_mask;
    uintptr_t potential_allocation_end = potential_allocation_begin + size;
    uintptr_t required_size = potential_allocation_end - current_buffer;

    if (required_size <= m_remaining_capacity) {
      m_remaining_capacity -= required_size;
      m_current_buffer = (void *)potential_allocation_end;
      return (void *)potential_allocation_begin;
    }
    else {
      this->allocate_new_buffer(size + alignment);
      return this->allocate(size, alignment);
    }
  };

  StringRefNull copy_string(StringRef str)
  {
    char *buffer = (char *)this->allocate(str.size() + 1, 1);
    str.copy_to__with_null(buffer);
    return StringRefNull((const char *)buffer);
  }

  template<typename T, typename... Args> destruct_ptr<T> construct(Args &&... args)
  {
    void *buffer = this->allocate(sizeof(T), alignof(T));
    T *value = new (buffer) T(std::forward<Args>(args)...);
    return destruct_ptr<T>(value);
  }

  template<typename T, typename... Args>
  ArrayRef<T *> construct_elements_and_pointer_array(uint n, Args &&... args)
  {
    void *pointer_buffer = this->allocate(n * sizeof(T *), alignof(T *));
    void *element_buffer = this->allocate(n * sizeof(T), alignof(T));

    MutableArrayRef<T *> pointers((T **)pointer_buffer, n);
    T *elements = (T *)element_buffer;

    for (uint i : IndexRange(n)) {
      pointers[i] = elements + i;
    }
    for (uint i : IndexRange(n)) {
      new (elements + i) T(std::forward<Args>(args)...);
    }

    return pointers;
  }

  template<typename T> MutableArrayRef<T> allocate_array_copy(ArrayRef<T> source)
  {
    T *buffer = (T *)this->allocate(source.byte_size(), alignof(T));
    source.copy_to(buffer);
    return MutableArrayRef<T>(buffer, source.size());
  }

 private:
  void allocate_new_buffer(uint min_allocation_size)
  {
    uint size_in_bytes = power_of_2_min_u(std::max(min_allocation_size, m_next_min_alloc_size));
    m_next_min_alloc_size = size_in_bytes * 2;

    void *buffer = m_allocator.allocate(size_in_bytes, __func__);
    m_pointers.append(buffer);
    m_remaining_capacity = size_in_bytes;
    m_current_buffer = buffer;
  }
};

}  // namespace BLI
