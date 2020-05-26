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

#ifndef __FN_VECTOR_ARRAY_REFS_HH__
#define __FN_VECTOR_ARRAY_REFS_HH__

#include "FN_array_refs.hh"

namespace FN {

template<typename T> class VirtualVectorArrayRef {
 private:
  enum Category {
    SingleArray,
    StartsAndSizes,
  };

  uint m_virtual_size;
  Category m_category;

  union {
    struct {
      ArrayRef<T> array;
    } single_array;
    struct {
      const T *const *starts;
      const uint *sizes;
    } starts_and_sizes;
  } m_data;

 public:
  VirtualVectorArrayRef()
  {
    m_virtual_size = 0;
    m_category = StartsAndSizes;
    m_data.starts_and_sizes.starts = nullptr;
    m_data.starts_and_sizes.sizes = nullptr;
  }

  VirtualVectorArrayRef(ArrayRef<T> array, uint virtual_size)
  {
    m_virtual_size = virtual_size;
    m_category = SingleArray;
    m_data.single_array.array = array;
  }

  VirtualVectorArrayRef(ArrayRef<const T *> starts, ArrayRef<uint> sizes)
  {
    BLI_assert(starts.size() == sizes.size());
    m_virtual_size = starts.size();
    m_category = StartsAndSizes;
    m_data.starts_and_sizes.starts = starts.begin();
    m_data.starts_and_sizes.sizes = sizes.begin();
  }

  uint size() const
  {
    return m_virtual_size;
  }

  VirtualArrayRef<T> operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);
    switch (m_category) {
      case SingleArray:
        return VirtualArrayRef<T>(m_data.single_array.array);
      case StartsAndSizes:
        return VirtualArrayRef<T>(ArrayRef<T>(m_data.starts_and_sizes.starts[index],
                                              m_data.starts_and_sizes.sizes[index]));
    }
    BLI_assert(false);
    return {};
  }
};

class GenericVirtualVectorArrayRef {
 private:
  enum Category {
    SingleArray,
    StartsAndSizes,
  };

  const CPPType *m_type;
  uint m_virtual_size;
  Category m_category;

  union {
    struct {
      const void *values;
      uint size;
    } single_array;
    struct {
      const void *const *starts;
      const uint *sizes;
    } starts_and_sizes;
  } m_data;

  GenericVirtualVectorArrayRef() = default;

 public:
  GenericVirtualVectorArrayRef(const CPPType &type)
  {
    m_type = &type;
    m_virtual_size = 0;
    m_category = StartsAndSizes;
    m_data.starts_and_sizes.starts = nullptr;
    m_data.starts_and_sizes.sizes = nullptr;
  }

  GenericVirtualVectorArrayRef(GenericArrayRef array, uint virtual_size)
  {
    m_type = &array.type();
    m_virtual_size = virtual_size;
    m_category = SingleArray;
    m_data.single_array.values = array.buffer();
    m_data.single_array.size = array.size();
  }

  GenericVirtualVectorArrayRef(const CPPType &type,
                               ArrayRef<const void *> starts,
                               ArrayRef<uint> sizes)
  {
    BLI_assert(starts.size() == sizes.size());
    m_type = &type;
    m_virtual_size = starts.size();
    m_category = StartsAndSizes;
    m_data.starts_and_sizes.starts = starts.begin();
    m_data.starts_and_sizes.sizes = sizes.begin();
  }

  uint size() const
  {
    return m_virtual_size;
  }

  GenericVirtualArrayRef operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);
    switch (m_category) {
      case SingleArray:
        return GenericVirtualArrayRef(
            GenericArrayRef(*m_type, m_data.single_array.values, m_data.single_array.size));
      case StartsAndSizes:
        return GenericVirtualArrayRef(GenericArrayRef(
            *m_type, m_data.starts_and_sizes.starts[index], m_data.starts_and_sizes.sizes[index]));
    }
    BLI_assert(false);
    return GenericVirtualArrayRef(*m_type);
  }
};

}  // namespace FN

#endif /* __FN_VECTOR_ARRAY_REFS_HH__ */
