#ifndef __BKE_GENERIC_VIRTUAL_LIST_REF_H__
#define __BKE_GENERIC_VIRTUAL_LIST_REF_H__

#include "BKE_cpp_types.h"

#include "BLI_virtual_list_ref.h"

namespace BKE {

using BLI::ArrayRef;
using BLI::VirtualListRef;

class GenericVirtualListRef {
 private:
  enum Category {
    Single,
    FullArray,
    RepeatedArray,
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
      const void *data;
      uint real_size;
    } repeated_array;
  } m_data;

  GenericVirtualListRef() = default;

 public:
  GenericVirtualListRef(const CPPType &type)
  {
    m_virtual_size = 0;
    m_type = &type;
    m_category = Category::FullArray;
    m_data.full_array.data = nullptr;
  }

  static GenericVirtualListRef FromSingle(const CPPType &type,
                                          const void *buffer,
                                          uint virtual_size)
  {
    GenericVirtualListRef list;
    list.m_virtual_size = virtual_size;
    list.m_type = &type;
    list.m_category = Single;
    list.m_data.single.data = buffer;
    return list;
  }

  static GenericVirtualListRef FromFullArray(const CPPType &type, const void *buffer, uint size)
  {
    GenericVirtualListRef list;
    list.m_virtual_size = size;
    list.m_type = &type;
    list.m_category = FullArray;
    list.m_data.full_array.data = buffer;
    return list;
  }

  template<typename T> static GenericVirtualListRef FromFullArray(ArrayRef<T> array)
  {
    return GenericVirtualListRef::FromFullArray(
        GET_TYPE<T>(), (const void *)array.begin(), array.size());
  }

  static GenericVirtualListRef FromRepeatedArray(const CPPType &type,
                                                 const void *buffer,
                                                 uint real_size,
                                                 uint virtual_size)
  {
    GenericVirtualListRef list;
    list.m_virtual_size = virtual_size;
    list.m_type = &type;
    list.m_category = RepeatedArray;
    list.m_data.repeated_array.data = buffer;
    list.m_data.repeated_array.real_size = real_size;
    return list;
  }

  uint size() const
  {
    return m_virtual_size;
  }

  const void *operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);
    switch (m_category) {
      case Single:
        return m_data.single.data;
      case FullArray:
        return POINTER_OFFSET(m_data.full_array.data, index * m_type->size());
      case RepeatedArray:
        uint real_index = index % m_data.repeated_array.real_size;
        return POINTER_OFFSET(m_data.repeated_array.data, real_index * m_type->size());
    }
    BLI_assert(false);
    return m_data.single.data;
  }

  template<typename T> VirtualListRef<T> as_typed_ref() const
  {
    BLI_assert(GET_TYPE<T>().is_same_or_generalization(*m_type));
    switch (m_category) {
      case Single:
        return VirtualListRef<T>::FromSingle((const T *)m_data.single.data, m_virtual_size);
      case FullArray:
        return VirtualListRef<T>::FromFullArray((const T *)m_data.full_array.data, m_virtual_size);
      case RepeatedArray:
        return VirtualListRef<T>::FromRepeatedArray((const T *)m_data.repeated_array.data,
                                                    m_data.repeated_array.real_size,
                                                    m_virtual_size);
    }
    BLI_assert(false);
    return {};
  }
};

}  // namespace BKE

#endif /* __BKE_GENERIC_VIRTUAL_LIST_REF_H__ */
