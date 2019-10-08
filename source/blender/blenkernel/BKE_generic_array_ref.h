#ifndef __BKE_GENERIC_ARRAY_REF_H__
#define __BKE_GENERIC_ARRAY_REF_H__

#include "BKE_cpp_type.h"
#include "BKE_cpp_types.h"

#include "BLI_array_ref.h"

namespace BKE {

using BLI::ArrayRef;
using BLI::MutableArrayRef;

class GenericArrayRef {
 private:
  const CPPType *m_type;
  void *m_buffer;
  uint m_size;

 public:
  GenericArrayRef(const CPPType *type) : GenericArrayRef(type, nullptr, 0)
  {
  }

  GenericArrayRef(const CPPType *type, void *buffer, uint size)
      : m_type(type), m_buffer(buffer), m_size(size)
  {
    BLI_assert(type != nullptr);
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type->pointer_has_valid_alignment(buffer));
  }

  uint size() const
  {
    return m_size;
  }

  template<typename T> ArrayRef<T> get_ref() const
  {
    BLI_assert(GET_TYPE<T>().is_same_or_generalization(m_type));
    return ArrayRef<T>((const T *)m_buffer, m_size);
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

  uint size() const
  {
    return m_size;
  }

  template<typename T> MutableArrayRef<T> get_ref()
  {
    BLI_assert(GET_TYPE<T>().is_same_or_generalization(m_type));
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
    new (ptr) GenericArrayRef(&self_->m_base_type);
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