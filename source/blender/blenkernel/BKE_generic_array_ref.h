#ifndef __BKE_GENERIC_ARRAY_REF_H__
#define __BKE_GENERIC_ARRAY_REF_H__

#include "BKE_cpp_type.h"
#include "BKE_cpp_types.h"

#include "BLI_array_ref.h"

namespace BKE {

using BLI::ArrayRef;

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
    BLI_assert(get_cpp_type<T>().is_same_or_generalization(m_type));
    return ArrayRef<T>((const T *)m_buffer, m_size);
  }
};

class ArrayRefCPPType : public CPPType {
 private:
  CPPType &m_base_type;

 public:
  ArrayRefCPPType(CPPType &base_type, CPPType &generalization);

  static void ConstructDefaultCB(const CPPType *self, void *ptr)
  {
    const ArrayRefCPPType *self_ = dynamic_cast<const ArrayRefCPPType *>(self);
    new (ptr) GenericArrayRef(&self_->m_base_type);
  }
};

ArrayRefCPPType &get_generic_array_ref_cpp_type(CPPType &base);

}  // namespace BKE

#endif /* __BKE_GENERIC_ARRAY_REF_H__ */