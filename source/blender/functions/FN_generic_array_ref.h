#ifndef __FN_GENERIC_ARRAY_REF_H__
#define __FN_GENERIC_ARRAY_REF_H__

#include "FN_cpp_type.h"

#include "BLI_array_ref.h"

namespace FN {

using BLI::ArrayRef;
using BLI::MutableArrayRef;

class GenericArrayRef {
 private:
  const CPPType *m_type;
  const void *m_buffer;
  uint m_size;

 public:
  GenericArrayRef(const CPPType &type) : GenericArrayRef(type, nullptr, 0)
  {
  }

  GenericArrayRef(const CPPType &type, const void *buffer, uint size)
      : m_type(&type), m_buffer(buffer), m_size(size)
  {
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type.pointer_has_valid_alignment(buffer));
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

  template<typename T> ArrayRef<T> as_typed_ref() const
  {
    BLI_assert(CPP_TYPE<T>().is_same_or_generalization(*m_type));
    return ArrayRef<T>((const T *)m_buffer, m_size);
  }
};

class GenericMutableArrayRef {
 private:
  const CPPType *m_type;
  void *m_buffer;
  uint m_size;

 public:
  GenericMutableArrayRef(const CPPType &type) : GenericMutableArrayRef(type, nullptr, 0)
  {
  }

  GenericMutableArrayRef(const CPPType &type, void *buffer, uint size)
      : m_type(&type), m_buffer(buffer), m_size(size)
  {
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type.pointer_has_valid_alignment(buffer));
  }

  template<typename T>
  GenericMutableArrayRef(ArrayRef<T> array)
      : GenericMutableArrayRef(CPP_TYPE<T>(), (void *)array.begin(), array.size())
  {
  }

  operator GenericArrayRef() const
  {
    return GenericArrayRef(*m_type, m_buffer, m_size);
  }

  void destruct_all()
  {
    if (m_type->trivially_destructible()) {
      return;
    }
    for (uint i = 0; i < m_size; i++) {
      m_type->destruct((*this)[i]);
    }
  }

  void destruct_indices(ArrayRef<uint> indices)
  {
    if (m_type->trivially_destructible()) {
      return;
    }
    for (uint i : indices) {
      m_type->destruct((*this)[i]);
    }
  }

  GenericMutableArrayRef slice(uint start, uint size)
  {
    BLI_assert(start + size <= m_size);
    return GenericMutableArrayRef(*m_type, POINTER_OFFSET(m_buffer, start * m_type->size()), size);
  }

  const CPPType &type() const
  {
    return *m_type;
  }

  void *buffer()
  {
    return m_buffer;
  }

  uint size() const
  {
    return m_size;
  }

  void default_initialize(ArrayRef<uint> indices)
  {
    for (uint i : indices) {
      m_type->construct_default((*this)[i]);
    }
  }

  void fill__uninitialized(const void *value)
  {
    m_type->fill_uninitialized(value, m_buffer, m_size);
  }

  void fill__initialized(const void *value)
  {
    m_type->fill_initialized(value, m_buffer, m_size);
  }

  void copy_in__uninitialized(uint index, const void *src)
  {
    BLI_assert(index < m_size);
    void *dst = POINTER_OFFSET(m_buffer, m_type->size() * index);
    m_type->copy_to_uninitialized(src, dst);
  }

  void copy_in__initialized(uint index, const void *src)
  {
    BLI_assert(index < m_size);
    void *dst = POINTER_OFFSET(m_buffer, m_type->size() * index);
    m_type->copy_to_initialized(src, dst);
  }

  static void RelocateUninitialized(GenericMutableArrayRef from, GenericMutableArrayRef to);

  void *operator[](uint index)
  {
    BLI_assert(index < m_size);
    return POINTER_OFFSET(m_buffer, m_type->size() * index);
  }

  template<typename T> MutableArrayRef<T> as_typed_ref()
  {
    BLI_assert(CPP_TYPE<T>().is_same_or_generalization(*m_type));
    return MutableArrayRef<T>((T *)m_buffer, m_size);
  }
};

}  // namespace FN

#endif /* __FN_GENERIC_ARRAY_REF_H__ */
