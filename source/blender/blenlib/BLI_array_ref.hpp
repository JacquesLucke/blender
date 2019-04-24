#pragma once

namespace BLI {

template<typename T> class ArrayRef {
 private:
  T *m_start = nullptr;
  uint m_size = 0;

 public:
  ArrayRef() = default;

  ArrayRef(T *start, uint size) : m_start(start), m_size(size)
  {
  }

  T *begin()
  {
    return m_start;
  }

  T *end()
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
};

template<typename ArrayT, typename ValueT, ValueT (*GetValue)(ArrayT &item)>
class StridedArrayRef {
 private:
  ArrayT *m_start = nullptr;
  uint m_size = 0;

 public:
  StridedArrayRef() = default;

  StridedArrayRef(ArrayT *start, uint size) : m_start(start), m_size(size)
  {
  }

  uint size() const
  {
    return m_size;
  }

  class It {
   private:
    StridedArrayRef m_array_ref;
    uint m_index;

   public:
    It(StridedArrayRef array_ref, uint index) : m_array_ref(array_ref), m_index(index)
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
