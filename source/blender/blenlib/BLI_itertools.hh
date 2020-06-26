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

#ifndef __BLI_ITERTOOLS_HH__
#define __BLI_ITERTOOLS_HH__

#include "BLI_utildefines.h"

namespace blender {

template<typename Int, typename Value, typename Iterator> class EnumerateIterator {
 private:
  uint m_index;
  Iterator m_current;

 public:
  struct Item {
    const Int index;
    const Value &value;
  };

  EnumerateIterator(Iterator current) : m_index(0), m_current(current)
  {
  }

  Item operator*() const
  {
    return {m_index, *m_current};
  }

  EnumerateIterator &operator++()
  {
    ++m_index;
    ++m_current;
    return *this;
  }

  friend bool operator!=(const EnumerateIterator &a, const EnumerateIterator &b)
  {
    return a.m_current != b.m_current;
  }
};

template<typename Iterator> class AnyRange {
 private:
  Iterator m_begin;
  Iterator m_end;

 public:
  AnyRange(Iterator begin, Iterator end) : m_begin(begin), m_end(end)
  {
  }

  Iterator begin() const
  {
    return m_begin;
  }

  Iterator end() const
  {
    return m_end;
  }
};

template<typename Int = uint, typename Container = void> auto enumerate(const Container &container)
{
  using Iterator =
      EnumerateIterator<Int, decltype(*container.begin()), decltype(container.begin())>;
  return AnyRange(Iterator(container.begin()), Iterator(container.end()));
}

}  // namespace blender

#endif /* __BLI_ITERTOOLS_HH__ */
