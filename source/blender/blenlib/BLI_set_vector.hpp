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

/** \file
 * \ingroup bli
 *
 * A set with small object optimization that keeps track
 * of insertion order. Internally, it is the same as Set
 * but that could potentially change in the future.
 */

#pragma once

#include "BLI_set.hpp"

namespace BLI {

template<typename T> class SetVector : public Set<T> {
 public:
  SetVector() : Set<T>()
  {
  }

  SetVector(const std::initializer_list<T> &values) : Set<T>(values)
  {
  }

  SetVector(const SmallVector<T> &values) : Set<T>(values)
  {
  }

  /**
   * Return the index of the value, or -1 when it does not exist.
   */
  int index(const T &value) const
  {
    return this->m_lookup.find(this->m_elements.begin(), value);
  }

  /**
   * Gives access to the underlying array of values.
   * The values should not be changed in ways that would modify their hash.
   */
  operator ArrayRef<T>() const
  {
    return ArrayRef<T>(this->begin(), this->size());
  }

  T &operator[](const int index) const
  {
    BLI_assert(index >= 0 && index < this->size());
    return this->m_elements[index];
  }
};

};  // namespace BLI
