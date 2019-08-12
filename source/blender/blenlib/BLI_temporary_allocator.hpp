#pragma once

#include "BLI_utildefines.h"
#include "BLI_vector_adaptor.hpp"

namespace BLI {

void *allocate_temp_buffer(uint size);
void free_temp_buffer(void *buffer);

template<typename T> T *allocate_temp_array(uint size)
{
  return (T *)allocate_temp_buffer(sizeof(T) * size);
}

class TemporaryBuffer {
 private:
  void *m_ptr;
  uint m_size = 0;

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

  /**
   * Take ownership over the pointer.
   */
  void *extract_ptr()
  {
    void *ptr = m_ptr;
    m_ptr = nullptr;
    m_size = 0;
    return ptr;
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
  TemporaryArray(uint size) : m_buffer(size * sizeof(T)), m_array((T *)m_buffer.ptr(), size)
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

  T &operator[](uint index)
  {
    return m_array[index];
  }

  T *ptr()
  {
    return (T *)m_buffer.ptr();
  }

  /**
   * Get the array ref and take ownership of the data.
   */
  ArrayRef<T> extract()
  {
    ArrayRef<T> array_ref = m_array;
    m_array = {};
    m_buffer.extract_ptr();
    return array_ref;
  }
};

}  // namespace BLI
