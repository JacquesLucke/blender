#pragma once

#include "BLI_array_ref.hpp"
#include "BLI_vector.hpp"

namespace BLI {

template<typename T, uint N = 4> class MultiVector {
 private:
  Vector<T, N> m_elements;
  Vector<uint> m_starts;

 public:
  MultiVector() : m_starts({0})
  {
  }

  void append(ArrayRef<T> values)
  {
    m_elements.extend(values);
    m_starts.append(m_elements.size());
  }

  uint size() const
  {
    return m_starts.size() - 1;
  }

  ArrayRef<T> operator[](uint index)
  {
    uint start = m_starts[index];
    uint one_after_end = m_starts[index + 1];
    uint size = one_after_end - start;
    return ArrayRef<T>(m_elements.begin() + start, size);
  }
};

};  // namespace BLI
