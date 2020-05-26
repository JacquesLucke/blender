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

#ifndef __FN_GENERIC_VIRTUAL_ARRAY_REF_HH__
#define __FN_GENERIC_VIRTUAL_ARRAY_REF_HH__

#include "FN_cpp_type.hh"
#include "FN_generic_array_ref.hh"
#include "FN_virtual_array_ref.hh"

namespace FN {

class GenericVirtualArrayRef {
 private:
  enum Category {
    Single,
    FullArray,
    FullPointerArray,
  };

  const CPPType *m_type;
  uint m_virtual_size;
  Category m_category;

  union {
    struct {
      const void *data;
    } single;
    struct {
      const void *data;
    } full_array;
    struct {
      const void *const *data;
    } full_pointer_array;
  } m_data;

  GenericVirtualArrayRef() = default;

 public:
  GenericVirtualArrayRef(const CPPType &type)
  {
    m_type = &type;
    m_virtual_size = 0;
    m_category = FullArray;
    m_data.full_array.data = nullptr;
  }

  GenericVirtualArrayRef(GenericArrayRef values)
  {
    m_type = &values.type();
    m_virtual_size = values.size();
    m_category = FullArray;
    m_data.full_array.data = values.buffer();
  }

  GenericVirtualArrayRef(GenericMutableArrayRef values)
      : GenericVirtualArrayRef(GenericArrayRef(values))
  {
  }

  template<typename T>
  GenericVirtualArrayRef(ArrayRef<T> values) : GenericVirtualArrayRef(GenericArrayRef(values))
  {
  }

  template<typename T>
  GenericVirtualArrayRef(MutableArrayRef<T> values)
      : GenericVirtualArrayRef(GenericArrayRef(values))
  {
  }

  static GenericVirtualArrayRef FromSingle(const CPPType &type,
                                           const void *value,
                                           uint virtual_size)
  {
    GenericVirtualArrayRef ref;
    ref.m_type = &type;
    ref.m_virtual_size = virtual_size;
    ref.m_category = Single;
    ref.m_data.single.data = value;
    return ref;
  }

  static GenericVirtualArrayRef FromFullPointerArray(const CPPType &type,
                                                     const void *const *values,
                                                     uint size)
  {
    GenericVirtualArrayRef ref;
    ref.m_type = &type;
    ref.m_virtual_size = size;
    ref.m_category = FullPointerArray;
    ref.m_data.full_pointer_array.data = values;
    return ref;
  }

  uint size() const
  {
    return m_virtual_size;
  }

  const CPPType &type() const
  {
    return *m_type;
  }

  const void *operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);
    switch (m_category) {
      case Single:
        return m_data.single.data;
      case FullArray:
        return POINTER_OFFSET(m_data.full_array.data, index * m_type->size());
      case FullPointerArray:
        return m_data.full_pointer_array.data[index];
    }
    BLI_assert(false);
    return m_data.single.data;
  }

  template<typename T> VirtualArrayRef<T> typed() const
  {
    BLI_assert(CPPType::get<T>() == *m_type);
    switch (m_category) {
      case Single:
        return VirtualArrayRef<T>::FromSingle((const T *)m_data.single.data, m_virtual_size);
      case FullArray:
        return VirtualArrayRef<T>(ArrayRef<T>((const T *)m_data.full_array.data, m_virtual_size));
      case FullPointerArray:
        return VirtualArrayRef<T>(
            ArrayRef<const T *>((const T *const *)m_data.full_pointer_array.data, m_virtual_size));
    }
    BLI_assert(false);
    return {};
  }
};

}  // namespace FN

#endif /* __FN_GENERIC_VIRTUAL_ARRAY_REF_HH__ */
