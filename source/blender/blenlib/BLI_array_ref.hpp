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

} /* namespace BLI */
