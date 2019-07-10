#pragma once

/* Allows passing iterators over ranges of integers without
 * actually allocating an array.
 */

#include "BLI_utildefines.h"
#include "BLI_small_vector.hpp"

#define RANGE_AS_ARRAY_REF_MAX_LEN 10000

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

  friend bool operator==(Range<T> a, Range<T> b)
  {
    return a.m_start == b.m_start && a.m_one_after_last == b.m_one_after_last;
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

  T first() const
  {
    BLI_assert(this->size() > 0);
    return m_start;
  }

  T last() const
  {
    BLI_assert(this->size() > 0);
    return m_one_after_last - 1;
  }

  T one_after_last() const
  {
    return m_one_after_last;
  }

  bool contains(T value) const
  {
    return value >= m_start && value < m_one_after_last;
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

  ArrayRef<T> as_array_ref() const;
};

template<typename T> class ChunkedRange {
 private:
  Range<T> m_total_range;
  uint m_chunk_size;
  uint m_chunk_amount;

 public:
  ChunkedRange(Range<T> total_range, uint chunk_size)
      : m_total_range(total_range),
        m_chunk_size(chunk_size),
        m_chunk_amount(std::ceil(m_total_range.size() / (float)m_chunk_size))
  {
  }

  uint chunks() const
  {
    return m_chunk_amount;
  }

  Range<T> chunk_range(uint index) const
  {
    BLI_assert(index < m_chunk_amount);
    T start = m_total_range[index * m_chunk_size];
    T one_after_last = std::min<T>(start + m_chunk_size, m_total_range.one_after_last());
    return Range<T>(start, one_after_last);
  }
};

}  // namespace BLI
