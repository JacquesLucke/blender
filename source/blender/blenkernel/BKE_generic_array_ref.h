#ifndef __BKE_GENERIC_ARRAY_REF_H__
#define __BKE_GENERIC_ARRAY_REF_H__

#include "BKE_cpp_type.h"
#include "BKE_cpp_types.h"

#include "BLI_array_ref.h"
#include "BLI_array_or_single_ref.h"

namespace BKE {

using BLI::ArrayOrSingleRef;
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

  template<typename T> ArrayRef<T> get_ref() const
  {
    BLI_assert(GET_TYPE<T>().is_same_or_generalization(*m_type));
    return ArrayRef<T>((const T *)m_buffer, m_size);
  }
};

class GenericArrayOrSingleRef {
 private:
  const CPPType *m_type;
  const void *m_buffer;
  uint m_array_size;
  bool m_is_single;

 public:
  GenericArrayOrSingleRef() = delete;

  GenericArrayOrSingleRef(const CPPType &type, const void *buffer, uint array_size, bool is_single)
      : m_type(&type), m_buffer(buffer), m_array_size(array_size), m_is_single(is_single)
  {
  }

  GenericArrayOrSingleRef(const CPPType &type) : GenericArrayOrSingleRef(type, nullptr, 0, false)
  {
  }

  static GenericArrayOrSingleRef FromSingle(const CPPType &type,
                                            const void *buffer,
                                            uint array_size)
  {
    return GenericArrayOrSingleRef(type, buffer, array_size, true);
  }

  static GenericArrayOrSingleRef FromArray(const CPPType &type,
                                           const void *buffer,
                                           uint array_size)
  {
    return GenericArrayOrSingleRef(type, buffer, array_size, false);
  }

  template<typename T> static GenericArrayOrSingleRef FromArray(ArrayRef<T> array)
  {
    return GenericArrayOrSingleRef::FromArray(
        GET_TYPE<T>(), (const void *)array.begin(), array.size());
  }

  template<typename T> ArrayOrSingleRef<T> as_typed_ref() const
  {
    BLI_assert(GET_TYPE<T>().is_same_or_generalization(*m_type));
    return ArrayOrSingleRef<T>((const T *)m_buffer, m_array_size, m_is_single);
  }

  const void *operator[](uint index) const
  {
    BLI_assert(index < m_array_size);
    if (m_is_single) {
      return m_buffer;
    }
    else {
      return POINTER_OFFSET(m_buffer, index * m_type->size());
    }
  }
};

class GenericMutableArrayRef {
 private:
  const CPPType *m_type;
  void *m_buffer;
  uint m_size;

 public:
  GenericMutableArrayRef(const CPPType *type) : GenericMutableArrayRef(type, nullptr, 0)
  {
  }

  GenericMutableArrayRef(const CPPType *type, void *buffer, uint size)
      : m_type(type), m_buffer(buffer), m_size(size)
  {
    BLI_assert(type != nullptr);
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type->pointer_has_valid_alignment(buffer));
  }

  template<typename T>
  GenericMutableArrayRef(ArrayRef<T> array)
      : GenericMutableArrayRef(&GET_TYPE<T>(), (void *)array.begin(), array.size())
  {
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

  void copy_in__uninitialized(uint index, const void *src)
  {
    BLI_assert(index < m_size);
    void *dst = POINTER_OFFSET(m_buffer, m_type->size() * index);
    m_type->copy_to_uninitialized(src, dst);
  }

  void *operator[](uint index)
  {
    BLI_assert(index < m_size);
    return POINTER_OFFSET(m_buffer, m_type->size() * index);
  }

  template<typename T> MutableArrayRef<T> get_ref()
  {
    BLI_assert(GET_TYPE<T>().is_same_or_generalization(*m_type));
    return MutableArrayRef<T>((T *)m_buffer, m_size);
  }
};

class ArrayRefCPPType final : public CPPType {
 private:
  CPPType &m_base_type;

 public:
  ArrayRefCPPType(CPPType &base_type);

  static void ConstructDefaultCB(const CPPType *self, void *ptr)
  {
    const ArrayRefCPPType *self_ = dynamic_cast<const ArrayRefCPPType *>(self);
    new (ptr) GenericArrayRef(self_->m_base_type);
  }
};

class MutableArrayRefCPPType final : public CPPType {
 private:
  CPPType &m_base_type;

 public:
  MutableArrayRefCPPType(CPPType &base_type);

  static void ConstructDefaultCB(const CPPType *self, void *ptr)
  {
    const MutableArrayRefCPPType *self_ = dynamic_cast<const MutableArrayRefCPPType *>(self);
    new (ptr) GenericMutableArrayRef(&self_->m_base_type);
  }
};

ArrayRefCPPType &GET_TYPE_array_ref(CPPType &base);
MutableArrayRefCPPType &GET_TYPE_mutable_array_ref(CPPType &base);

template<typename T> ArrayRefCPPType &GET_TYPE_array_ref()
{
  return GET_TYPE_array_ref(GET_TYPE<T>());
}

template<typename T> MutableArrayRefCPPType &GET_TYPE_mutable_array_ref()
{
  return GET_TYPE_mutable_array_ref(GET_TYPE<T>());
}

}  // namespace BKE

#endif /* __BKE_GENERIC_ARRAY_REF_H__ */
