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

#ifndef __FN_SPANS_HH__
#define __FN_SPANS_HH__

#include "BLI_span.hh"

#include "FN_cpp_type.hh"

namespace blender {
namespace fn {

class GenericSpan {
 private:
  const CPPType *m_type;
  const void *m_buffer;
  uint m_size;

 public:
  GenericSpan(const CPPType &type, const void *buffer, uint size)
      : m_type(&type), m_buffer(buffer), m_size(size)
  {
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type.pointer_has_valid_alignment(buffer));
  }

  GenericSpan(const CPPType &type) : GenericSpan(type, nullptr, 0)
  {
  }

  template<typename T>
  GenericSpan(Span<T> array)
      : GenericSpan(CPPType::get<T>(), (const void *)array.begin(), array.size())
  {
  }

  const CPPType &type() const
  {
    return *m_type;
  }

  bool is_empty() const
  {
    return m_size == 0;
  }

  uint size() const
  {
    return m_size;
  }

  const void *buffer() const
  {
    return m_buffer;
  }

  const void *operator[](uint index) const
  {
    BLI_assert(index < m_size);
    return POINTER_OFFSET(m_buffer, m_type->size() * index);
  }

  template<typename T> Span<T> typed() const
  {
    BLI_assert(CPPType::get<T>() == *m_type);
    return Span<T>((const T *)m_buffer, m_size);
  }
};

class GenericMutableSpan {
 private:
  const CPPType *m_type;
  void *m_buffer;
  uint m_size;

 public:
  GenericMutableSpan(const CPPType &type, void *buffer, uint size)
      : m_type(&type), m_buffer(buffer), m_size(size)
  {
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type.pointer_has_valid_alignment(buffer));
  }

  GenericMutableSpan(const CPPType &type) : GenericMutableSpan(type, nullptr, 0)
  {
  }

  template<typename T>
  GenericMutableSpan(MutableSpan<T> array)
      : GenericMutableSpan(CPPType::get<T>(), (void *)array.begin(), array.size())
  {
  }

  operator GenericSpan() const
  {
    return GenericSpan(*m_type, m_buffer, m_size);
  }

  const CPPType &type() const
  {
    return *m_type;
  }

  bool is_empty() const
  {
    return m_size == 0;
  }

  uint size() const
  {
    return m_size;
  }

  void *buffer()
  {
    return m_buffer;
  }

  void *operator[](uint index)
  {
    BLI_assert(index < m_size);
    return POINTER_OFFSET(m_buffer, m_type->size() * index);
  }

  template<typename T> MutableSpan<T> typed()
  {
    BLI_assert(CPPType::get<T>() == *m_type);
    return MutableSpan<T>((T *)m_buffer, m_size);
  }
};

template<typename T> class VirtualSpan {
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
  VirtualSpan()
  {
    m_virtual_size = 0;
    m_category = FullArray;
    m_data.full_array.data = nullptr;
  }

  VirtualSpan(Span<T> values)
  {
    m_virtual_size = values.size();
    m_category = FullArray;
    m_data.full_array.data = values.begin();
  }

  VirtualSpan(MutableSpan<T> values) : VirtualSpan(Span<T>(values))
  {
  }

  VirtualSpan(Span<const T *> values)
  {
    m_virtual_size = values.size();
    m_category = FullPointerArray;
    m_data.full_pointer_array.data = values.begin();
  }

  static VirtualSpan FromSingle(const T *value, uint virtual_size)
  {
    VirtualSpan ref;
    ref.m_virtual_size = virtual_size;
    ref.m_category = Single;
    ref.m_data.single.data = value;
    return ref;
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

  bool is_empty() const
  {
    return m_virtual_size == 0;
  }

  uint size() const
  {
    return m_virtual_size;
  }
};

class GenericVirtualSpan {
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

  GenericVirtualSpan() = default;

 public:
  GenericVirtualSpan(const CPPType &type)
  {
    m_type = &type;
    m_virtual_size = 0;
    m_category = FullArray;
    m_data.full_array.data = nullptr;
  }

  GenericVirtualSpan(GenericSpan values)
  {
    m_type = &values.type();
    m_virtual_size = values.size();
    m_category = FullArray;
    m_data.full_array.data = values.buffer();
  }

  GenericVirtualSpan(GenericMutableSpan values) : GenericVirtualSpan(GenericSpan(values))
  {
  }

  template<typename T> GenericVirtualSpan(Span<T> values) : GenericVirtualSpan(GenericSpan(values))
  {
  }

  template<typename T>
  GenericVirtualSpan(MutableSpan<T> values) : GenericVirtualSpan(GenericSpan(values))
  {
  }

  static GenericVirtualSpan FromSingle(const CPPType &type, const void *value, uint virtual_size)
  {
    GenericVirtualSpan ref;
    ref.m_type = &type;
    ref.m_virtual_size = virtual_size;
    ref.m_category = Single;
    ref.m_data.single.data = value;
    return ref;
  }

  static GenericVirtualSpan FromFullPointerArray(const CPPType &type,
                                                 const void *const *values,
                                                 uint size)
  {
    GenericVirtualSpan ref;
    ref.m_type = &type;
    ref.m_virtual_size = size;
    ref.m_category = FullPointerArray;
    ref.m_data.full_pointer_array.data = values;
    return ref;
  }

  bool is_empty() const
  {
    return m_virtual_size == 0;
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

  template<typename T> VirtualSpan<T> typed() const
  {
    BLI_assert(CPPType::get<T>() == *m_type);
    switch (m_category) {
      case Single:
        return VirtualSpan<T>::FromSingle((const T *)m_data.single.data, m_virtual_size);
      case FullArray:
        return VirtualSpan<T>(Span<T>((const T *)m_data.full_array.data, m_virtual_size));
      case FullPointerArray:
        return VirtualSpan<T>(
            Span<const T *>((const T *const *)m_data.full_pointer_array.data, m_virtual_size));
    }
    BLI_assert(false);
    return {};
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_SPANS_HH__ */
