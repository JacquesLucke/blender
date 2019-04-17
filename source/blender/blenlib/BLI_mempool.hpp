#pragma once

#include "BLI_small_stack.hpp"

namespace BLI {

class MemPool {
 private:
  SmallStack<void *> m_free_stack;
  SmallVector<void *> m_start_pointers;
  uint m_element_size;

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

  void *allocate()
  {
    if (m_free_stack.empty()) {
      this->allocate_more();
    }
    return m_free_stack.pop();
  }

  void deallocate(void *ptr)
  {
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