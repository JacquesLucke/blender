#ifndef __BLI_VIRTUAL_ARRAY_LIST_REF_H__
#define __BLI_VIRTUAL_ARRAY_LIST_REF_H__

#include "BLI_virtual_list_ref.h"

namespace BLI {

template<typename T> class VirtualListListRef {
 private:
  enum Category {
    SingleArray,
    ListOfStartPointers,
  };

  uint m_virtual_size;
  Category m_category;

  union {
    struct {
      const T *start;
      uint size;
    } single_array;
    struct {
      const T *const *starts;
      const uint *sizes;
    } list_of_start_pointers;
  } m_data;

 public:
  VirtualListListRef()
  {
    m_virtual_size = 0;
    m_category = ListOfStartPointers;
    m_data.list_of_start_pointers.starts = nullptr;
    m_data.list_of_start_pointers.sizes = nullptr;
  }

  static VirtualListListRef FromSingleArray(ArrayRef<T> array, uint virtual_list_size)
  {
    VirtualListListRef list;
    list.m_virtual_size = virtual_list_size;
    list.m_category = Category::SingleArray;
    list.m_data.single_array.start = array.begin();
    list.m_data.single_array.size = array.size();
    return list;
  }

  static VirtualListListRef FromListOfStartPointers(ArrayRef<const T *> starts,
                                                    ArrayRef<uint> sizes)
  {
    BLI_assert(starts.size() == sizes.size());
    VirtualListListRef list;
    list.m_virtual_size = starts.size();
    list.m_category = Category::ListOfStartPointers;
    list.m_data.list_of_start_pointers.starts = starts.begin();
    list.m_data.list_of_start_pointers.sizes = sizes.begin();
    return list;
  }

  uint size() const
  {
    return m_virtual_size;
  }

  VirtualListRef<T> operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);

    switch (m_category) {
      case Category::SingleArray:
        return VirtualListRef<T>::FromFullArray(
            ArrayRef<T>(m_data.single_array.start, m_data.single_array.size));
      case Category::ListOfStartPointers:
        return VirtualListRef<T>::FromFullArray(m_data.list_of_start_pointers.starts[index],
                                                m_data.list_of_start_pointers.sizes[index]);
    }

    BLI_assert(false);
    return {};
  }
};

}  // namespace BLI

#endif /* __BLI_VIRTUAL_ARRAY_LIST_REF_H__ */
