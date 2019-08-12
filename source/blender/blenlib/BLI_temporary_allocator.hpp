#pragma once

#include "BLI_utildefines.h"
#include "BLI_vector_adaptor.hpp"

namespace BLI {

void *allocate_temp_buffer(uint size);
void free_temp_buffer(void *buffer);

class TemporaryBuffer {
 private:
  void *m_ptr;
  uint m_size;

 public:
  TemporaryBuffer(uint size) : m_ptr(allocate_temp_buffer(size)), m_size(size)
  {
  }

  TemporaryBuffer(const TemporaryBuffer &other) = delete;
  TemporaryBuffer(TemporaryBuffer &&other) : m_ptr(other.m_ptr)
  {
    other.m_ptr = nullptr;
  }

  ~TemporaryBuffer()
  {
    if (m_ptr != nullptr) {
      free_temp_buffer(m_ptr);
    }
  }

  TemporaryBuffer &operator=(TemporaryBuffer &other) = delete;
  TemporaryBuffer &operator=(TemporaryBuffer &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->~TemporaryBuffer();
    new (this) TemporaryBuffer(std::move(other));
    return *this;
  }

  uint size() const
  {
    return m_size;
  }

  void *ptr() const
  {
    return m_ptr;
  }
};

template<typename T> class TemporaryVector {
 private:
  TemporaryBuffer m_buffer;
  VectorAdaptor<T> m_vector;

 public:
  TemporaryVector(uint capacity)
      : m_buffer(capacity * sizeof(T)), m_vector((T *)m_buffer.ptr(), capacity)
  {
  }

  ~TemporaryVector()
  {
    m_vector.clear();
  }

  operator VectorAdaptor<T> &()
  {
    return m_vector;
  }

  operator ArrayRef<T>()
  {
    return m_vector;
  }

  T &operator[](uint index)
  {
    return m_vector[index];
  }

  VectorAdaptor<T> *operator->()
  {
    return &m_vector;
  }
};

template<typename T> class TemporaryArray {
 private:
  TemporaryBuffer m_buffer;
  ArrayRef<T> m_array;

 public:
  TemporaryArray(uint size) : m_buffer(size * sizeof(T)), m_array((T *)m_buffer, size)
  {
  }

  operator ArrayRef<T>()
  {
    return m_array;
  }

  ArrayRef<T> *operator->()
  {
    return &m_array;
  }
};

}  // namespace BLI
