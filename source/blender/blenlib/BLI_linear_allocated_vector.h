#pragma once

#include "BLI_index_range.h"
#include "BLI_linear_allocator.h"
#include "BLI_memory_utils_cxx.h"

namespace BLI {

template<typename T> class LinearAllocatedVector : BLI::NonCopyable {
 private:
  T *m_begin;
  T *m_end;
  T *m_capacity_end;

#ifdef DEBUG
  uint m_debug_size;
#  define UPDATE_VECTOR_SIZE(ptr) (ptr)->m_debug_size = (ptr)->m_end - (ptr)->m_begin
#else
#  define UPDATE_VECTOR_SIZE(ptr) ((void)0)
#endif

 public:
  LinearAllocatedVector() : m_begin(nullptr), m_end(nullptr), m_capacity_end(nullptr)
  {
    UPDATE_VECTOR_SIZE(this);
  }

  ~LinearAllocatedVector()
  {
    destruct_n(m_begin, this->size());
  }

  LinearAllocatedVector(LinearAllocatedVector &&other)
  {
    m_begin = other.m_begin;
    m_end = other.m_end;
    m_capacity_end = other.m_capacity_end;

    other.m_begin = nullptr;
    other.m_end = nullptr;
    other.m_capacity_end = nullptr;

    UPDATE_VECTOR_SIZE(this);
    UPDATE_VECTOR_SIZE(&other);
  }

  LinearAllocatedVector &operator=(LinearAllocatedVector &&other)
  {
    if (this == &other) {
      return *this;
    }

    m_begin = other.m_begin;
    m_end = other.m_end;
    m_capacity_end = other.m_capacity_end;

    other.m_begin = nullptr;
    other.m_end = nullptr;
    other.m_capacity_end = nullptr;

    UPDATE_VECTOR_SIZE(this);
    UPDATE_VECTOR_SIZE(&other);

    return *this;
  }

  operator ArrayRef<T>() const
  {
    return ArrayRef<T>(m_begin, this->size());
  }

  operator MutableArrayRef<T>()
  {
    return MutableArrayRef<T>(m_begin, this->size());
  }

  ArrayRef<T> as_ref() const
  {
    return *this;
  }

  MutableArrayRef<T> as_mutable_ref() const
  {
    return *this;
  }

  IndexRange index_range() const
  {
    return IndexRange(this->size());
  }

  uint size() const
  {
    return m_end - m_begin;
  }

  uint capacity() const
  {
    return m_capacity_end - m_begin;
  }

  void clear()
  {
    destruct_n(m_begin, this->size());
    m_end = m_begin;
    UPDATE_VECTOR_SIZE(this);
  }

  void append_unchecked(const T &value)
  {
    BLI_assert(m_end < m_capacity_end);
    new (m_end) T(value);
    m_end++;
    UPDATE_VECTOR_SIZE(this);
  }

  template<typename AllocT> void append(const T &value, LinearAllocator<AllocT> &allocator)
  {
    if (m_end == m_capacity_end) {
      this->grow(this->size() + 1, allocator);
    }
    this->append_unchecked(value);
  }

  template<typename AllocT>
  uint append_and_get_index(const T &value, LinearAllocator<AllocT> &allocator)
  {
    uint index = this->size();
    this->append(value, allocator);
    return index;
  }

  bool contains(const T &value) const
  {
    for (const T &current : *this) {
      if (current == value) {
        return true;
      }
    }
    return false;
  }

  const T &operator[](uint index) const
  {
    BLI_assert(index < this->size());
    return m_begin[index];
  }

  T &operator[](uint index)
  {
    BLI_assert(index < this->size());
    return m_begin[index];
  }

  const T *begin() const
  {
    return m_begin;
  }

  const T *end() const
  {
    return m_end;
  }

  T *begin()
  {
    return m_begin;
  }

  T *end()
  {
    return m_end;
  }

  void remove_and_reorder(uint index)
  {
    BLI_assert(index < this->size());
    T *element_to_remove = m_begin + index;
    m_end--;
    if (element_to_remove < m_end) {
      *element_to_remove = std::move(*m_end);
    }
    destruct(m_end);
    UPDATE_VECTOR_SIZE(this);
  }

  int index_try(const T &value) const
  {
    for (T *current = m_begin; current != m_end; current++) {
      if (*current == value) {
        return current - m_begin;
      }
    }
    return -1;
  }

  uint index(const T &value) const
  {
    int index = this->index_try(value);
    BLI_assert(index >= 0);
    return (uint)index;
  }

  void remove_first_occurrence_and_reorder(const T &value)
  {
    uint index = this->index(value);
    this->remove_and_reorder((uint)index);
  }

 private:
  template<typename AllocT>
  BLI_NOINLINE void grow(uint min_capacity, LinearAllocator<AllocT> &allocator)
  {
    if (min_capacity <= this->capacity()) {
      return;
    }

    uint size = this->size();
    min_capacity = power_of_2_max_u(min_capacity);

    T *new_begin = (T *)allocator.allocate(sizeof(T) * min_capacity, alignof(T));
    uninitialized_relocate_n(m_begin, size, new_begin);

    m_begin = new_begin;
    m_end = new_begin + size;
    m_capacity_end = new_begin + min_capacity;
  }
};

}  // namespace BLI
