#pragma once

/* Basic stack implementation with support for
 * small object optimization.
 */

#include "BLI_small_vector.hpp"

namespace BLI {

template<typename T, uint N = 4> class SmallStack {
 private:
  SmallVector<T, N> m_elements;

 public:
  SmallStack() = default;

  uint size() const
  {
    return m_elements.size();
  }

  bool empty() const
  {
    return this->size() == 0;
  }

  void push(T value)
  {
    m_elements.append(value);
  }

  T pop()
  {
    BLI_assert(!this->empty());
    T value = m_elements[this->size() - 1];
    m_elements.remove_last();
    return value;
  }

  T &peek()
  {
    BLI_assert(!this->empty());
    return m_elements[this->size() - 1];
  }

  T *begin() const
  {
    return m_elements.begin();
  }

  T *end() const
  {
    return m_elements.end();
  }
};

} /* namespace BLI */
