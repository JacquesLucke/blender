#ifndef __BKE_GENERIC_ARRAY_REF_H__
#define __BKE_GENERIC_ARRAY_REF_H__

#include "BKE_type_cpp.h"
#include "BKE_types_cpp.h"

#include "BLI_array_ref.h"

namespace BKE {

using BLI::ArrayRef;

class GenericArrayRef {
 private:
  const TypeCPP *m_type;
  void *m_buffer;
  uint m_size;

 public:
  GenericArrayRef(const TypeCPP *type) : GenericArrayRef(type, nullptr, 0)
  {
  }

  GenericArrayRef(const TypeCPP *type, void *buffer, uint size)
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
    BLI_assert(get_type_cpp<T>().is_same_or_generalization(m_type));
    return ArrayRef<T>((const T *)m_buffer, m_size);
  }
};

class ArrayRefTypeCPP : public TypeCPP {
 private:
  TypeCPP &m_base_type;

 public:
  ArrayRefTypeCPP(TypeCPP &base_type, TypeCPP &generalization);

  static void ConstructDefaultCB(const TypeCPP *self, void *ptr)
  {
    const ArrayRefTypeCPP *self_ = dynamic_cast<const ArrayRefTypeCPP *>(self);
    new (ptr) GenericArrayRef(&self_->m_base_type);
  }
};

ArrayRefTypeCPP &get_generic_array_ref_cpp_type(TypeCPP &base);

}  // namespace BKE

#endif /* __BKE_GENERIC_ARRAY_REF_H__ */