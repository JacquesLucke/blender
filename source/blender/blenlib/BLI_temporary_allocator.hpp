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
 * This allocation method should be used when a chunk of memory is only used for a short amount
 * of time. This makes it possible to cache potentially large buffers for reuse, without the fear
 * of running out of memory because too many large buffers are allocated.
 *
 * Many cpu-bound algorithms can benefit from being split up into several stages, whereby the
 * output of one stage is written into an array that is read by the next stage. This improves
 * debugability as well as profilability. Often a reason this is not done is that the memory
 * allocation might be expensive. The goal of this allocator is to make this a non-issue, by
 * reusing the same long buffers over and over again.
 *
 * The number of allocated buffers should stay in O(number of threads * max depth of stack trace).
 * Since these numbers are pretty much constant in Blender, the number of chunks allocated should
 * not increase over time.
 */

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
