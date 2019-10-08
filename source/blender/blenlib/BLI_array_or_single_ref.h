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

#ifndef __BLI_ARRAY_OR_SINGLE_REF_H__
#define __BLI_ARRAY_OR_SINGLE_REF_H__

/** \file
 * \ingroup bli
 */

#include "BLI_array_ref.h"

namespace BLI {

template<typename T> class ArrayOrSingleRef {
 private:
  const T *m_buffer;
  uint m_array_size;
  bool m_is_single;

  ArrayOrSingleRef(const T *buffer, uint array_size, bool is_single)
      : m_buffer(buffer), m_array_size(array_size), m_is_single(is_single)
  {
  }

 public:
  ArrayOrSingleRef() : ArrayOrSingleRef(nullptr, 0, false)
  {
  }

  ArrayOrSingleRef(ArrayRef<T> array) : ArrayOrSingleRef(array.begin(), array.size(), false)
  {
  }

  static ArrayOrSingleRef FromSingle(const T *value, uint array_size)
  {
    return ArrayOrSingleRef(value, array_size, true);
  }

  static ArrayOrSingleRef FromArray(const T *value, uint size)
  {
    return ArrayOrSingleRef(value, size, false);
  }

  const T &operator[](uint index) const
  {
    BLI_assert(index <= m_array_size);
    if (m_is_single) {
      return *m_buffer;
    }
    else {
      return m_buffer[index];
    }
  }

  uint size() const
  {
    return m_array_size;
  }
};

}  // namespace BLI

#endif /* __BLI_ARRAY_OR_SINGLE_REF_H__ */
