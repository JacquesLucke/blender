#ifndef __FN_GENERIC_VIRTUAL_LIST_LIST_REF_H__
#define __FN_GENERIC_VIRTUAL_LIST_LIST_REF_H__

#include "BLI_virtual_list_list_ref.h"

#include "FN_generic_virtual_list_ref.h"

namespace FN {

using BLI::VirtualListListRef;

class GenericVirtualListListRef {
 private:
  enum Category {
    SingleArray,
    FullArrayList,
  };

  const CPPType *m_type;
  uint m_virtual_list_size;
  Category m_category;

  union {
    struct {
      const void *data;
      uint real_array_size;
    } single_array;
    struct {
      const void *const *starts;
      const uint *real_array_sizes;
    } full_array_list;
  } m_data;

  GenericVirtualListListRef() = default;

 public:
  static GenericVirtualListListRef FromSingleArray(const CPPType &type,
                                                   const void *buffer,
                                                   uint real_array_size,
                                                   uint virtual_list_size)
  {
    GenericVirtualListListRef list;
    list.m_type = &type;
    list.m_virtual_list_size = virtual_list_size;
    list.m_category = Category::SingleArray;
    list.m_data.single_array.data = buffer;
    list.m_data.single_array.real_array_size = real_array_size;
    return list;
  }

  static GenericVirtualListListRef FromFullArrayList(const CPPType &type,
                                                     const void *const *starts,
                                                     const uint *real_array_sizes,
                                                     uint list_size)
  {
    GenericVirtualListListRef list;
    list.m_type = &type;
    list.m_virtual_list_size = list_size;
    list.m_category = Category::FullArrayList;
    list.m_data.full_array_list.starts = starts;
    list.m_data.full_array_list.real_array_sizes = real_array_sizes;
    return list;
  }

  static GenericVirtualListListRef FromFullArrayList(const CPPType &type,
                                                     ArrayRef<const void *> starts,
                                                     ArrayRef<uint> array_sizes)
  {
    BLI_assert(starts.size() == array_sizes.size());
    return GenericVirtualListListRef::FromFullArrayList(
        type, starts.begin(), array_sizes.begin(), starts.size());
  }

  uint size() const
  {
    return m_virtual_list_size;
  }

  GenericVirtualListRef operator[](uint index) const
  {
    BLI_assert(index < m_virtual_list_size);

    switch (m_category) {
      case Category::SingleArray:
        return GenericVirtualListRef::FromFullArray(
            *m_type, m_data.single_array.data, m_data.single_array.real_array_size);
      case Category::FullArrayList:
        return GenericVirtualListRef::FromFullArray(
            *m_type,
            m_data.full_array_list.starts[index],
            m_data.full_array_list.real_array_sizes[index]);
    }

    BLI_assert(false);
    return GenericVirtualListRef{*m_type};
  }

  template<typename T> VirtualListListRef<T> as_typed_ref() const
  {
    BLI_assert(GET_TYPE<T>().is_same_or_generalization(*m_type));

    switch (m_category) {
      case Category::SingleArray:
        return VirtualListListRef<T>::FromSingleArray(
            ArrayRef<T>((const T *)m_data.single_array.data, m_data.single_array.real_array_size),
            m_virtual_list_size);
      case Category::FullArrayList:
        return VirtualListListRef<T>::FromListOfStartPointers(
            ArrayRef<const T *>((const T **)m_data.full_array_list.starts, m_virtual_list_size),
            ArrayRef<uint>(m_data.full_array_list.real_array_sizes, m_virtual_list_size));
    }

    BLI_assert(false);
    return {};
  }

  GenericVirtualListRef repeated_sublist(uint index, uint new_virtual_size) const
  {
    BLI_assert(index < m_virtual_list_size);

    switch (m_category) {
      case Category::SingleArray:
        return GenericVirtualListRef::FromRepeatedArray(*m_type,
                                                        m_data.single_array.data,
                                                        m_data.single_array.real_array_size,
                                                        new_virtual_size);
      case Category::FullArrayList:
        return GenericVirtualListRef::FromRepeatedArray(
            *m_type,
            m_data.full_array_list.starts[index],
            m_data.full_array_list.real_array_sizes[index],
            new_virtual_size);
    }

    BLI_assert(false);
    return {*m_type};
  }
};

}  // namespace FN

#endif /* __FN_GENERIC_VIRTUAL_LIST_LIST_REF_H__ */
