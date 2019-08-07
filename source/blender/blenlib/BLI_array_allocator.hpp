#pragma once

/**
 * This allocator should be used, when arrays of the same length are often allocated and
 * deallocated. Knowing that all arrays have the same length makes it possible to just store the
 * size of a single element to identify the buffer length, which is a small number usually.
 */

#include "BLI_stack.hpp"
#include "BLI_vector_adaptor.hpp"

namespace BLI {

class ArrayAllocator {
 private:
  Vector<void *, 16> m_all_pointers;
  Vector<Stack<void *>, 16> m_pointer_stacks;
  uint m_array_length;

 public:
  /**
   * Create a new allocator that will allocate arrays with the given length (the element size may
   * vary).
   */
  ArrayAllocator(uint array_length) : m_array_length(array_length)
  {
  }

  ArrayAllocator(ArrayAllocator &other) = delete;

  ~ArrayAllocator()
  {
    for (void *ptr : m_all_pointers) {
      MEM_freeN(ptr);
    }
  }

  /**
   * Get the number of elements in the arrays allocated by this allocator.
   */
  uint array_size() const
  {
    return m_array_length;
  }

  /**
   * Allocate an array buffer in which every element has the given size.
   */
  void *allocate(uint element_size)
  {
    Stack<void *> &stack = this->stack_for_element_size(element_size);
    if (stack.size() > 0) {
      return stack.pop();
    }
    void *ptr = MEM_mallocN_aligned(m_array_length * element_size, 64, __func__);
    m_all_pointers.append(ptr);
    return ptr;
  }

  /**
   * Deallocate an array buffer that has been allocated with this allocator before.
   */
  void deallocate(void *ptr, uint element_size)
  {
    Stack<void *> &stack = this->stack_for_element_size(element_size);
    BLI_assert(!stack.contains(ptr));
    stack.push(ptr);
  }

  /**
   * Allocate a new array of the given type.
   */
  template<typename T> T *allocate()
  {
    return (T *)this->allocate(sizeof(T));
  }

  /**
   * Deallocate an array of the given type. It has to be allocated with this allocator before.
   */
  template<typename T> void deallocate(T *ptr)
  {
    return this->deallocate(ptr, sizeof(T));
  }

  /**
   * A wrapper for allocated arrays so that they will be deallocated automatically when they go out
   * of scope.
   */
  template<typename T> class ScopedAllocation {
   private:
    ArrayAllocator &m_allocator;
    void *m_ptr;
    uint m_element_size;

   public:
    ScopedAllocation(ArrayAllocator &allocator, T *ptr, uint element_size)
        : m_allocator(allocator), m_ptr(ptr), m_element_size(element_size)
    {
    }

    ScopedAllocation(ScopedAllocation &other) = delete;
    ScopedAllocation(ScopedAllocation &&other)
        : m_allocator(other.m_allocator), m_ptr(other.m_ptr), m_element_size(other.m_element_size)
    {
      other.m_ptr = nullptr;
    }

    ScopedAllocation &operator=(ScopedAllocation &other) = delete;
    ScopedAllocation &operator=(ScopedAllocation &&other)
    {
      this->~ScopedAllocation();
      new (this) ScopedAllocation(std::move(other));
      return *this;
    }

    ~ScopedAllocation()
    {
      if (m_ptr != nullptr) {
        m_allocator.deallocate(m_ptr, m_element_size);
      }
    }

    operator T *() const
    {
      return (T *)m_ptr;
    }

    ArrayAllocator &allocator()
    {
      return m_allocator;
    }
  };

  /**
   * Allocate an array with the given element size. The return value is a wrapper around the
   * pointer, so that it is automatically deallocated.
   */
  ScopedAllocation<void> allocate_scoped(uint element_size)
  {
    return ScopedAllocation<void>(*this, this->allocate(element_size), element_size);
  }

  /**
   * Allocate an array of the given type. The return value is a wrapper around the pointer, so that
   * it is automatically deallocated.
   */
  template<typename T> ScopedAllocation<T> allocate_scoped()
  {
    return ScopedAllocation<T>(*this, this->allocate<T>(), sizeof(T));
  }

  /**
   * This is a simple vector that has been allocated using an array allocator. The maximum size of
   * the vector is determined by the allocator.
   */
  template<typename T> class VectorAdapter {
   private:
    ScopedAllocation<T> m_ptr;
    VectorAdaptor<T> m_vector;

   public:
    VectorAdapter(ArrayAllocator &allocator)
        : m_ptr(allocator.allocate_scoped<T>()), m_vector(m_ptr, allocator.array_size())
    {
    }

    ~VectorAdapter() = default;

    operator VectorAdaptor<T> &()
    {
      return m_vector;
    }

    operator ArrayRef<T>()
    {
      return m_vector;
    }
  };

  /**
   * This is a simple fixed size array that has been allocated using an array allocator.
   */
  template<typename T> class Array {
   private:
    ScopedAllocation<T> m_ptr;
    uint m_size;

   public:
    Array(ArrayAllocator &allocator) : Array(allocator, allocator.array_size())
    {
    }

    Array(ArrayAllocator &allocator, uint size)
        : m_ptr(allocator.allocate_scoped<T>()), m_size(size)
    {
      BLI_assert(size <= allocator.array_size());
    }

    operator ArrayRef<T>()
    {
      return ArrayRef<T>(m_ptr, m_size);
    }

    T &operator[](uint index)
    {
      return ((T *)m_ptr)[index];
    }

    ArrayRef<T> as_array_ref()
    {
      return ArrayRef<T>(m_ptr, m_size);
    }
  };

 private:
  Stack<void *> &stack_for_element_size(uint element_size)
  {
    BLI_assert(element_size > 0);
    uint index = element_size - 1;
    if (index >= m_pointer_stacks.size()) {
      m_pointer_stacks.append_n_times({}, index - m_pointer_stacks.size() + 1);
    }
    return m_pointer_stacks[index];
  }
};

};  // namespace BLI
