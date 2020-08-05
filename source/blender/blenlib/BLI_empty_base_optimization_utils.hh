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

#pragma once

#include "BLI_memory_utils.hh"

namespace blender {

template<typename T1, typename T2> class CompressedPair_BothEmpty : private T1, private T2 {
 public:
  CompressedPair_BothEmpty() = default;

  T1 &first()
  {
    return *this;
  }

  const T1 &first() const
  {
    return *this;
  }

  T2 &second()
  {
    return *this;
  }

  const T2 &second() const
  {
    return *this;
  }
};

template<typename T1, typename T2> class CompressedPair_FirstEmpty : private T1 {
 private:
  T2 second_;

 public:
  CompressedPair_FirstEmpty() = default;

  T1 &first()
  {
    return *this;
  }

  const T1 &first() const
  {
    return *this;
  }

  T2 &second()
  {
    return second_;
  }

  const T2 &second() const
  {
    return second_;
  }
};

template<typename T1, typename T2> class CompressedPair_SecondEmpty : private T2 {
 private:
  T1 first_;

 public:
  CompressedPair_SecondEmpty() = default;

  T1 &first()
  {
    return first_;
  }

  const T1 &first() const
  {
    return first_;
  }

  T2 &second()
  {
    return *this;
  }

  const T2 &second() const
  {
    return *this;
  }
};

template<typename T1, typename T2> class CompressedPair_NoneEmpty {
 private:
  T1 first_;
  T2 second_;

 public:
  CompressedPair_NoneEmpty() = default;

  T1 &first()
  {
    return first_;
  }

  const T1 &first() const
  {
    return first_;
  }

  T2 &second()
  {
    return second_;
  }

  const T2 &second() const
  {
    return second_;
  }
};

template<typename T1, typename T2>
using CompressedPair = std::conditional_t<std::is_empty_v<T1>,
                                          std::conditional_t<std::is_empty_v<T2>,
                                                             CompressedPair_BothEmpty<T1, T2>,
                                                             CompressedPair_FirstEmpty<T1, T2>>,
                                          std::conditional_t<std::is_empty_v<T2>,
                                                             CompressedPair_SecondEmpty<T1, T2>,
                                                             CompressedPair_NoneEmpty<T1, T2>>>;

}  // namespace blender
