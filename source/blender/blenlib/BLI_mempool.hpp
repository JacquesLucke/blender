#pragma once

/* Use this memory allocator when:
 *   - all allocations have the same size
 *   - only a single thread allocates from this allocator
 *   - all allocated memory should be returned to the system at once
 *
 * The allocator keeps track of all unused allocated chunks
 * in a stack. Allocation pops the top chunk, while deallocation
 * pushes the chunk back to the stack.
 *
 * Memory is never returned to the system in this allocator.
 * If the task requires that to happen, another allocator should be
 * used, so that this allocator can stay simple.
 *
 * allocate: O(1) amortized
 * deallocate: O(1)
 * internal allocations: O(lg n) where n is the number of allocations
 *
 */

#include "BLI_stack.hpp"
#include "BLI_set.hpp"

namespace BLI {

class MemPool {
 private:
  Stack<void *> m_free_stack;
  Vector<void *> m_start_pointers;
  uint m_element_size;

#ifdef DEBUG
  Set<void *> m_allocated_pointers;
#endif

 public:
  MemPool(uint element_size) : m_element_size(element_size)
  {
  }

  MemPool(MemPool &mempool) = delete;

  ~MemPool()
  {
    for (void *ptr : m_start_pointers) {
      MEM_freeN(ptr);
    }
  }

  /**
   * Get a pointer to an uninitialized memory buffer of the size set in the constructor. The buffer
   * will be invalidated when the MemPool is destroyed.
   */
  void *allocate()
  {
    if (m_free_stack.empty()) {
      this->allocate_more();
    }
    void *ptr = m_free_stack.pop();
#ifdef DEBUG
    m_allocated_pointers.add_new(ptr);
#endif
    return ptr;
  }

  /**
   * Deallocate a pointer that has been allocated using the same mempool before. The memory won't
   * actually be freed immediatly.
   */
  void deallocate(void *ptr)
  {
#ifdef DEBUG
    BLI_assert(m_allocated_pointers.contains(ptr));
    m_allocated_pointers.remove(ptr);
#endif
    m_free_stack.push(ptr);
  }

  void print_stats() const
  {
    std::cout << "MemPool at " << (void *)this << std::endl;
    std::cout << "  Free Amount: " << m_free_stack.size() << std::endl;
    std::cout << "  Allocations: " << m_start_pointers.size() << std::endl;
  }

 private:
  void allocate_more()
  {
    uint new_amount = 1 << (m_start_pointers.size() + 4);
    void *ptr = MEM_malloc_arrayN(new_amount, m_element_size, __func__);

    for (uint i = 0; i < new_amount; i++) {
      m_free_stack.push((char *)ptr + i * m_element_size);
    }

    m_start_pointers.append(ptr);
  }
};

} /* namespace BLI */
