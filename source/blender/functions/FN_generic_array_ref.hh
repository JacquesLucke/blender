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

#ifndef __FN_GENERIC_ARRAY_REF_HH__
#define __FN_GENERIC_ARRAY_REF_HH__

/** \file
 * \ingroup functions
 *
 * GenericArrayRef and GenericMutableArrayRef are almost equivalent to their statically typed
 * counterparts BLI::ArrayRef and BLI::MutableArrayRef. The only difference is that these generic
 * variants also store a pointer to a CPPType instance.
 */

#include "FN_cpp_type.hh"

namespace FN {

using BLI::ArrayRef;
using BLI::MutableArrayRef;

class GenericArrayRef {
 private:
  const CPPType *m_type;
  const void *m_buffer;
  uint m_size;

 public:
  GenericArrayRef(const CPPType &type, const void *buffer, uint size)
      : m_type(&type), m_buffer(buffer), m_size(size)
  {
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type.pointer_has_valid_alignment(buffer));
  }

  GenericArrayRef(const CPPType &type) : GenericArrayRef(type, nullptr, 0)
  {
  }

  template<typename T>
  GenericArrayRef(ArrayRef<T> array)
      : GenericArrayRef(CPPType::get<T>(), (const void *)array.begin(), array.size())
  {
  }

  const CPPType &type() const
  {
    return *m_type;
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

  template<typename T> ArrayRef<T> typed() const
  {
    BLI_assert(CPPType::get<T>() == *m_type);
    return ArrayRef<T>((const T *)m_buffer, m_size);
  }
};

class GenericMutableArrayRef {
 private:
  const CPPType *m_type;
  void *m_buffer;
  uint m_size;

 public:
  GenericMutableArrayRef(const CPPType &type, void *buffer, uint size)
      : m_type(&type), m_buffer(buffer), m_size(size)
  {
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type.pointer_has_valid_alignment(buffer));
  }

  GenericMutableArrayRef(const CPPType &type) : GenericMutableArrayRef(type, nullptr, 0)
  {
  }

  template<typename T>
  GenericMutableArrayRef(MutableArrayRef<T> array)
      : GenericMutableArrayRef(CPPType::get<T>(), (void *)array.begin(), array.size())
  {
  }

  operator GenericArrayRef() const
  {
    return GenericArrayRef(*m_type, m_buffer, m_size);
  }

  const CPPType &type() const
  {
    return *m_type;
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

  template<typename T> MutableArrayRef<T> typed()
  {
    BLI_assert(CPPType::get<T>() == *m_type);
    return MutableArrayRef<T>((T *)m_buffer, m_size);
  }
};

}  // namespace FN

#endif /* __FN_GENERIC_ARRAY_REF_HH__ */
