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

#ifndef __FN_VIRTUAL_ARRAY_REF_HH__
#define __FN_VIRTUAL_ARRAY_REF_HH__

#include "BLI_array_ref.hh"

namespace FN {

using BLI::ArrayRef;

template<typename T> class VirtualArrayRef {
 private:
  enum Category {
    Single,
    FullArray,
    FullPointerArray,
  };

  uint m_virtual_size;
  Category m_category;

  union {
    struct {
      const T *data;
    } single;
    struct {
      const T *data;
    } full_array;
    struct {
      const T *const *data;
    } full_pointer_array;
  } m_data;

 public:
  VirtualArrayRef()
  {
    m_virtual_size = 0;
    m_category = FullArray;
    m_data.full_array.data = nullptr;
  }

  static VirtualArrayRef FromSingle(const T *data, uint virtual_size)
  {
    VirtualArrayRef ref;
    ref.m_virtual_size = virtual_size;
    ref.m_category = Single;
    ref.m_data.single.data = data;
    return ref;
  }

  static VirtualArrayRef FromFullArray(const T *data, uint size)
  {
    VirtualArrayRef ref;
    ref.m_virtual_size = size;
    ref.m_category = FullArray;
    ref.m_data.full_array.data = data;
    return ref;
  }

  static VirtualArrayRef FromFullArray(ArrayRef<T> data)
  {
    return VirtualArrayRef::FromFullArray(data.begin(), data.size());
  }

  static VirtualArrayRef FromFullPointerArray(const T *const *data, uint size)
  {
    VirtualArrayRef ref;
    ref.m_virtual_size = size;
    ref.m_category = FullPointerArray;
    ref.m_data.full_pointer_array.data = data;
    return ref;
  }

  static VirtualArrayRef FromFullPointerArray(ArrayRef<const T *> data)
  {
    return VirtualArrayRef::FromFullPointerArray(data.begin(), data.size());
  }

  const T &operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);
    switch (m_category) {
      case Single:
        return *m_data.single.data;
      case FullArray:
        return m_data.full_array.data[index];
      case FullPointerArray:
        return *m_data.full_pointer_array.data[index];
    }
    BLI_assert(false);
    return *m_data.single.data;
  }

  uint size() const
  {
    return m_virtual_size;
  }
};

}  // namespace FN

#endif /* __FN_VIRTUAL_ARRAY_REF_HH__ */
