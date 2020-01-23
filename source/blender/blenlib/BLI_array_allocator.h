#ifndef __BLI_ARRAY_ALLOCATOR_H__
#define __BLI_ARRAY_ALLOCATOR_H__

#include "BLI_vector.h"
#include "BLI_stack_cxx.h"
#include "BLI_map.h"
#include "BLI_utility_mixins.h"

namespace BLI {

class ArrayAllocator : NonCopyable, NonMovable {
 private:
  uint m_array_size;

  Vector<Stack<void *>, 16> m_buffers;
  Vector<void *, 16> m_all_buffers;

#ifdef DEBUG
  Map<void *, uint> m_element_size_by_buffer;
#endif

 public:
  ArrayAllocator(uint array_size) : m_array_size(array_size)
  {
  }

  ~ArrayAllocator()
  {
#ifdef DEBUG
    uint buffer_count = 0;
    for (Stack<void *> &stack : m_buffers) {
      buffer_count += stack.size();
    }
    /* Make sure all arrays have been deallocated before the allocator is destructed. */
    BLI_assert(m_all_buffers.size() == buffer_count);
#endif

    for (void *buffer : m_all_buffers) {
      MEM_freeN(buffer);
    }
  }

  uint array_size() const
  {
    return m_array_size;
  }

  void *allocate(uint element_size, uint alignment)
  {
    BLI_assert(alignment <= 64);
    UNUSED_VARS_NDEBUG(alignment);

    Stack<void *> &stack = this->stack_for_element_size(element_size);
    if (stack.is_empty()) {
      void *new_buffer = MEM_mallocN_aligned(
          m_array_size * element_size, 64, "allocate in ArrayAllocator");
      m_all_buffers.append(new_buffer);
      stack.push(new_buffer);
#ifdef DEBUG
      m_element_size_by_buffer.add_new(new_buffer, element_size);
#endif
    }

    return stack.pop();
  }

  void deallocate(uint element_size, void *buffer)
  {
#ifdef DEBUG
    uint actual_element_size = m_element_size_by_buffer.lookup(buffer);
    BLI_assert(element_size == actual_element_size);
#endif

    Stack<void *> &stack = this->stack_for_element_size(element_size);
    BLI_assert(!stack.contains(buffer));
    stack.push(buffer);
  }

  template<typename T> MutableArrayRef<T> allocate()
  {
    T *buffer = (T *)this->allocate(sizeof(T), alignof(T));
    return MutableArrayRef<T>(buffer, m_array_size);
  }

 private:
  Stack<void *> &stack_for_element_size(uint element_size)
  {
    if (UNLIKELY(element_size > m_buffers.size())) {
      uint missing_amount = element_size - m_buffers.size();
      m_buffers.append_n_times({}, missing_amount);
    }
    return m_buffers[element_size - 1];
  }
};

}  // namespace BLI

#endif /* __BLI_ARRAY_ALLOCATOR_H__ */
