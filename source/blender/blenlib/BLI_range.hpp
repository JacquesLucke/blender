#pragma once

/* Allows passing iterators over ranges of integers without
 * actually allocating an array.
 */

#include "BLI_utildefines.h"
#include "BLI_small_vector.hpp"

namespace BLI {

template<typename T> class Range {
 private:
  T m_start = 0;
  T m_one_after_last = 0;

 public:
  Range() = default;

  Range(T start, T one_after_last) : m_start(start), m_one_after_last(one_after_last)
  {
    BLI_assert(start <= one_after_last);
  }

  class RangeIterator {
   private:
    const Range &m_range;
    T m_current;

   public:
    RangeIterator(const Range &range, T current) : m_range(range), m_current(current)
    {
    }

    RangeIterator &operator++()
    {
      m_current++;
      return *this;
    }

    bool operator!=(const RangeIterator &iterator) const
    {
      return m_current != iterator.m_current;
    }

    T operator*() const
    {
      return m_current;
    }
  };

  RangeIterator begin() const
  {
    return RangeIterator(*this, m_start);
  }

  RangeIterator end() const
  {
    return RangeIterator(*this, m_one_after_last);
  }

  T operator[](uint index) const
  {
    BLI_assert(index < this->size());
    return m_start + index;
  }

  uint size() const
  {
    return m_one_after_last - m_start;
  }

  Range after(uint n) const
  {
    return Range(m_one_after_last, m_one_after_last + n);
  }

  Range before(uint n) const
  {
    return Range(m_start - n, m_start);
  }

  BLI_NOINLINE SmallVector<T> to_small_vector() const
  {
    SmallVector<T> values;
    values.reserve(this->size());
    for (T value : *this) {
      values.append(value);
    }
    return values;
  }
};
}  // namespace BLI
