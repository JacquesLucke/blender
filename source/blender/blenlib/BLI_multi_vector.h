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
 * A vector that can group consecutive elements. This is much more efficient than allocating many
 * vectors separately.
 *
 * Note: the number of elements per group cannot be changed cheaply afterwards.
 */

#pragma once

#include "BLI_array_ref.h"
#include "BLI_vector.h"

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

}  // namespace BLI
