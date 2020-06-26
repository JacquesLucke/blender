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

#include <iostream>
#include <tuple>

namespace blender {

template<typename Int, typename Container> class Enumerate {
 private:
  Container m_container;
  Int m_start;

 public:
  template<typename T>
  Enumerate(T &&container, Int start) : m_container(std::forward<T>(container)), m_start(start)
  {
  }

  using Iter = decltype(m_container.begin());
  using Value = decltype(*std::declval<Iter>());

  struct Item {
    Int index;
    Value value;

    friend std::ostream &operator<<(std::ostream &stream, const Item &item)
    {
      stream << "(" << item.index << ", " << item.value << ")";
      return stream;
    }
  };

  class Iterator {
   private:
    Int m_index;
    Iter m_current;

   public:
    Iterator(Int index, Iter current) : m_index(index), m_current(current)
    {
    }

    Iterator &operator++()
    {
      ++m_current;
      ++m_index;
      return *this;
    }

    Item operator*() const
    {
      return {m_index, *m_current};
    }

    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      return a.m_current != b.m_current;
    }
  };

  Iterator begin()
  {
    return Iterator(m_start, m_container.begin());
  }

  Iterator end()
  {
    return Iterator(0, m_container.end());
  }
};

template<typename Int = uint, typename Container = void>
Enumerate<Int, Container> enumerate(Container &&container, Int start = 0)
{
  return {std::forward<Container>(container), start};
}

template<typename Container1, typename Container2> class ZipEqual2 {
 private:
  Container1 m_container1;
  Container2 m_container2;

 public:
  template<typename T1, typename T2>
  ZipEqual2(T1 &&container1, T2 &&container2)
      : m_container1(std::forward<T1>(container1)), m_container2(std::forward<T2>(container2))
  {
  }

  using Iter1 = decltype(m_container1.begin());
  using Iter2 = decltype(m_container2.begin());
  using Value1 = decltype(*std::declval<Iter1>());
  using Value2 = decltype(*std::declval<Iter2>());

  struct Item {
    Value1 value1;
    Value2 value2;

    friend std::ostream &operator<<(std::ostream &stream, const Item &item)
    {
      stream << "(" << item.value1 << ", " << item.value2 << ")";
      return stream;
    }
  };

  class Iterator {
   private:
    Iter1 m_current1;
    Iter2 m_current2;

   public:
    Iterator(Iter1 current1, Iter2 current2) : m_current1(current1), m_current2(current2)
    {
    }

    Iterator &operator++()
    {
      ++m_current1;
      ++m_current2;
      return *this;
    }

    Item operator*() const
    {
      return {*m_current1, *m_current2};
    }

    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      BLI_assert((a.m_current1 != b.m_current1) == (a.m_current2 != b.m_current2));
      return a.m_current1 != b.m_current1;
    }
  };

  Iterator begin()
  {
    return Iterator(m_container1.begin(), m_container2.begin());
  }

  Iterator end()
  {
    return Iterator(m_container1.end(), m_container2.end());
  }
};

// template<typename Container1, typename Container2>
// ZipEqual2<Container1, Container2> zip_equal(Container1 &&container1, Container2 &&container2)
// {
//   return {std::forward<Container1>(container1), std::forward<Container2>(container2)};
// }

template<typename... Container> class ZipEqual {
 private:
  std::tuple<Container...> m_containers;

 public:
  template<typename... T>
  ZipEqual(T &&... containers) : m_containers(std::forward<T>(containers)...)
  {
  }

  using Item = std::tuple<decltype(*std::declval<Container>().begin())...>;

  class Iterator {
   private:
    using Iterators = std::tuple<decltype(std::declval<Container>().begin())...>;
    Iterators m_current;

   public:
    Iterator(Iterators current) : m_current(current)
    {
    }

    Iterator &operator++()
    {
      std::apply([](auto &... current) { ((++current), ...); }, m_current);
      return *this;
    }

    Item operator*() const
    {
      return std::apply([](auto &... x) { return Item(*x...); }, m_current);
    }

    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      return std::get<0>(a.m_current) != std::get<0>(b.m_current);
    }
  };

  Iterator begin()
  {
    return {std::apply([](auto &... x) { return std::make_tuple(x.begin()...); }, m_containers)};
  }

  Iterator end()
  {
    return {std::apply([](auto &... x) { return std::make_tuple(x.end()...); }, m_containers)};
  }
};

template<typename... Container> ZipEqual<Container...> zip_equal(Container &&... args)
{
  return {std::forward<Container>(args)...};
}

}  // namespace blender

#endif /* __BLI_ITERTOOLS_HH__ */
