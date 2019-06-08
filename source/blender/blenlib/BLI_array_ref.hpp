#pragma once

/* An ArrayRef references some memory buffer owned
 * by someone else. If possible, functions should take
 * an ArrayRef as input. This allows passing on different
 * kinds of class types without doing unnecessary conversions.
 */

#include <vector>
#include <array>

#include "BLI_small_vector.hpp"

namespace BLI {

template<typename T> class ArrayRef {
 private:
  T *m_start = nullptr;
  uint m_size = 0;

 public:
  ArrayRef() = default;

  ArrayRef(T &value) : m_start(&value), m_size(1)
  {
  }

  ArrayRef(T *start, uint size) : m_start(start), m_size(size)
  {
  }

  ArrayRef(const T *start, uint size) : m_start((T *)start), m_size(size)
  {
  }

  template<uint N>
  ArrayRef(const SmallVector<T, N> &vector) : ArrayRef(vector.begin(), vector.size())
  {
  }

  ArrayRef(const SmallVector<T> &vector) : ArrayRef(vector.begin(), vector.size())
  {
  }

  ArrayRef(const std::initializer_list<T> &list) : ArrayRef((T *)list.begin(), list.size())
  {
  }

  ArrayRef(const std::vector<T> &vector) : ArrayRef(vector.data(), vector.size())
  {
  }

  template<std::size_t N> ArrayRef(const std::array<T, N> &array) : ArrayRef(array.data(), N)
  {
  }

  ArrayRef slice(uint start, uint length) const
  {
    BLI_assert(start + length <= this->size());
    return ArrayRef(m_start + start, length);
  }

  ArrayRef drop_front(uint n = 1) const
  {
    BLI_assert(n <= this->size());
    return this->slice(n, this->size() - n);
  }

  ArrayRef drop_back(uint n = 1) const
  {
    BLI_assert(n <= this->size());
    return this->slice(0, this->size() - n);
  }

  void fill(const T &element)
  {
    std::fill_n(m_start, m_size, element);
  }

  T *begin() const
  {
    return m_start;
  }

  T *end() const
  {
    return m_start + m_size;
  }

  T &operator[](uint index) const
  {
    BLI_assert(index < m_size);
    return m_start[index];
  }

  uint size() const
  {
    return m_size;
  }

  bool contains(const T &value)
  {
    for (T &element : *this) {
      if (element == value) {
        return true;
      }
    }
    return false;
  }

  uint count(const T &value)
  {
    uint counter = 0;
    for (T &element : *this) {
      if (element == value) {
        counter++;
      }
    }
    return counter;
  }

  template<uint N = 4> SmallVector<T, N> to_small_vector() const
  {
    SmallVector<T, N> vector;
    for (T &value : *this) {
      vector.append(value);
    }
    return vector;
  }
};

template<typename ArrayT, typename ValueT, ValueT (*GetValue)(ArrayT &item)> class MappedArrayRef {
 private:
  ArrayT *m_start = nullptr;
  uint m_size = 0;

 public:
  MappedArrayRef() = default;

  MappedArrayRef(ArrayT *start, uint size) : m_start(start), m_size(size)
  {
  }

  uint size() const
  {
    return m_size;
  }

  class It {
   private:
    MappedArrayRef m_array_ref;
    uint m_index;

   public:
    It(MappedArrayRef array_ref, uint index) : m_array_ref(array_ref), m_index(index)
    {
    }

    It &operator++()
    {
      m_index++;
      return *this;
    }

    bool operator!=(const It &other) const
    {
      BLI_assert(m_array_ref.m_start == other.m_array_ref.m_start);
      return m_index != other.m_index;
    }

    ValueT operator*() const
    {
      return GetValue(m_array_ref.m_start[m_index]);
    }
  };

  It begin() const
  {
    return It(*this, 0);
  }

  It end() const
  {
    return It(*this, m_size);
  }
};

} /* namespace BLI */
