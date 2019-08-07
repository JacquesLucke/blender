#pragma once

/**
 * A monotonic allocator is the simplest form of an allocator. It never reuses any memory, and
 * therefore does not need a deallocation method. It simply hands out consecutive buffers of
 * memory. When the current buffer is full, it reallocates a new larger buffer and continues.
 */

#include "BLI_vector.hpp"

namespace BLI {

class MonotonicAllocator {
 private:
  Vector<void *> m_pointers;

  void *m_current_buffer;
  uint m_remaining_capacity;
  uint m_next_min_alloc_size;

 public:
  MonotonicAllocator()
      : m_current_buffer(nullptr), m_remaining_capacity(0), m_next_min_alloc_size(16)
  {
  }

  MonotonicAllocator(MonotonicAllocator &other) = delete;
  MonotonicAllocator(MonotonicAllocator &&other) = delete;

  ~MonotonicAllocator()
  {
    for (void *ptr : m_pointers) {
      MEM_freeN(ptr);
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
      uint byte_size = std::max(m_next_min_alloc_size, size);
      void *ptr = MEM_mallocN(byte_size, __func__);
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

  template<typename T> ArrayRef<T> allocate_array(uint length)
  {
    return ArrayRef<T>((T *)this->allocate(sizeof(T) * length), length);
  }
};

}  // namespace BLI
