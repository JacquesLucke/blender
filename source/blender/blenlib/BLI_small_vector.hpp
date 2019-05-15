#pragma once

/* This vector wraps a dynamically sized array of a specific type.
 * It supports small object optimization. That means, when the
 * vector only contains a few elements, no extra memory allocation
 * is performed. Instead, those elements are stored directly in
 * the vector.
 */

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"
#include <cstdlib>
#include <cstring>
#include <memory>
#include <iostream>
#include <algorithm>

namespace BLI {

template<typename T> void uninitialized_relocate_n(T *src, uint n, T *dst)
{
  std::uninitialized_copy_n(std::make_move_iterator(src), n, dst);
  for (uint i = 0; i < n; i++) {
    src[i].~T();
  }
}

template<typename T, uint N = 4> class SmallVector {
 private:
  char m_small_buffer[sizeof(T) * N];
  T *m_elements;
  uint m_size = 0;
  uint m_capacity = N;

 public:
  SmallVector()
  {
    m_elements = this->small_buffer();
    m_capacity = N;
    m_size = 0;
  }

  SmallVector(uint size) : SmallVector()
  {
    this->reserve(size);
    for (uint i = 0; i < size; i++) {
      new (this->element_ptr(i)) T();
    }
    m_size = size;
  }

  SmallVector(std::initializer_list<T> values) : SmallVector()
  {
    this->reserve(values.size());
    for (T value : values) {
      this->append(value);
    }
  }

  SmallVector(const SmallVector &other)
  {
    this->copy_from_other(other);
  }

  SmallVector(SmallVector &&other)
  {
    this->steal_from_other(std::forward<SmallVector>(other));
  }

  ~SmallVector()
  {
    this->destruct_elements_and_free_memory();
  }

  SmallVector &operator=(const SmallVector &other)
  {
    if (this == &other) {
      return *this;
    }

    this->destruct_elements_and_free_memory();
    this->copy_from_other(other);

    return *this;
  }

  SmallVector &operator=(SmallVector &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->destruct_elements_and_free_memory();
    this->steal_from_other(std::forward<SmallVector>(other));

    return *this;
  }

  void reserve(uint size)
  {
    this->grow(size);
  }

  void clear()
  {
    this->destruct_elements_but_keep_memory();
    this->m_size = 0;
  }

  void append(const T &value)
  {
    this->ensure_space_for_one();
    std::uninitialized_copy_n(&value, 1, this->end());
    m_size++;
  }

  void append(T &&value)
  {
    this->ensure_space_for_one();
    std::uninitialized_copy_n(std::make_move_iterator(&value), 1, this->end());
    m_size++;
  }

  void extend(const SmallVector &other)
  {
    for (const T &value : other) {
      this->append(value);
    }
  }

  void extend(const T *start, uint amount)
  {
    for (uint i = 0; i < amount; i++) {
      this->append(start[i]);
    }
  }

  void fill(const T &value)
  {
    for (uint i = 0; i < m_size; i++) {
      m_elements[i] = value;
    }
  }

  uint size() const
  {
    return m_size;
  }

  bool empty() const
  {
    return this->size() == 0;
  }

  void remove_last()
  {
    BLI_assert(!this->empty());
    this->destruct_element(m_size - 1);
    m_size--;
  }

  T pop_last()
  {
    BLI_assert(!this->empty());
    T value = m_elements[this->size() - 1];
    this->remove_last();
    return value;
  }

  void remove_and_reorder(uint index)
  {
    BLI_assert(this->is_index_in_range(index));
    if (index < m_size - 1) {
      /* Move last element to index. */
      std::copy(std::make_move_iterator(this->end() - 1),
                std::make_move_iterator(this->end()),
                this->element_ptr(index));
    }
    this->destruct_element(m_size - 1);
    m_size--;
  }

  int index(const T &value) const
  {
    for (uint i = 0; i < m_size; i++) {
      if (m_elements[i] == value) {
        return i;
      }
    }
    return -1;
  }

  bool contains(const T &value) const
  {
    return this->index(value) != -1;
  }

  static bool all_equal(const SmallVector &a, const SmallVector &b)
  {
    if (a.size() != b.size()) {
      return false;
    }
    for (uint i = 0; i < a.size(); i++) {
      if (a[i] != b[i]) {
        return false;
      }
    }
    return true;
  }

  T &operator[](const int index) const
  {
    BLI_assert(this->is_index_in_range(index));
    return m_elements[index];
  }

  T *begin() const
  {
    return m_elements;
  }
  T *end() const
  {
    return this->begin() + this->size();
  }

  const T *cbegin() const
  {
    return this->begin();
  }
  const T *cend() const
  {
    return this->end();
  }

  void print_stats() const
  {
    std::cout << "Small Vector at " << (void *)this << ":" << std::endl;
    std::cout << "  Elements: " << this->size() << std::endl;
    std::cout << "  Capacity: " << this->m_capacity << std::endl;
    std::cout << "  Small Elements: " << N << "  Size on Stack: " << sizeof(*this) << std::endl;
  }

 private:
  T *small_buffer() const
  {
    return (T *)m_small_buffer;
  }

  bool is_small() const
  {
    return m_elements == this->small_buffer();
  }

  bool is_index_in_range(uint index) const
  {
    return index < this->size();
  }

  T *element_ptr(uint index) const
  {
    return m_elements + index;
  }

  inline void ensure_space_for_one()
  {
    if (m_size >= m_capacity) {
      this->grow(std::max(m_capacity * 2, (uint)1));
    }
  }

  void grow(uint min_capacity)
  {
    if (m_capacity >= min_capacity) {
      return;
    }

    m_capacity = min_capacity;

    T *new_array = (T *)MEM_malloc_arrayN(m_capacity, sizeof(T), __func__);
    uninitialized_relocate_n(m_elements, m_size, new_array);

    if (!this->is_small()) {
      MEM_freeN(m_elements);
    }

    m_elements = new_array;
  }

  void copy_from_other(const SmallVector &other)
  {
    if (other.is_small()) {
      m_elements = this->small_buffer();
    }
    else {
      m_elements = (T *)MEM_malloc_arrayN(other.m_capacity, sizeof(T), __func__);
    }

    std::uninitialized_copy(other.begin(), other.end(), m_elements);
    m_capacity = other.m_capacity;
    m_size = other.m_size;
  }

  void steal_from_other(SmallVector &&other)
  {
    if (other.is_small()) {
      uninitialized_relocate_n(other.begin(), other.size(), this->small_buffer());
      m_elements = this->small_buffer();
    }
    else {
      m_elements = other.m_elements;
    }

    m_capacity = other.m_capacity;
    m_size = other.m_size;

    other.m_size = 0;
    other.m_capacity = N;
    other.m_elements = other.small_buffer();
  }

  void destruct_elements_and_free_memory()
  {
    this->destruct_elements_but_keep_memory();
    if (!this->is_small()) {
      MEM_freeN(m_elements);
    }
  }

  void destruct_elements_but_keep_memory()
  {
    for (uint i = 0; i < m_size; i++) {
      this->destruct_element(i);
    }
  }

  void destruct_element(uint index)
  {
    this->element_ptr(index)->~T();
  }
};

} /* namespace BLI */
