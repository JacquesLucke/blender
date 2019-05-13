#pragma once

/* A set with small object optimization that keeps track
 * of insertion order. Internally, it is the same as SmallSet
 * but that could potentially change in the future.
 */

#include "BLI_small_set.hpp"

namespace BLI {

template<typename T> class SmallSetVector : public SmallSet<T> {
 public:
  SmallSetVector() : SmallSet<T>()
  {
  }

  SmallSetVector(const std::initializer_list<T> &values) : SmallSet<T>(values)
  {
  }

  SmallSetVector(const SmallVector<T> &values) : SmallSet<T>(values)
  {
  }

  int index(const T &value) const
  {
    return this->m_lookup.find(this->m_elements.begin(), value);
  }

  T operator[](const int index) const
  {
    BLI_assert(index >= 0 && index < this->size());
    return this->m_elements[index];
  }
};

};  // namespace BLI
