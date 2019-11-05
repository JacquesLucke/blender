#ifndef __BLI_VIRTUAL_LIST_REF_H__
#define __BLI_VIRTUAL_LIST_REF_H__

#include "BLI_array_ref.h"

#include <climits>

namespace BLI {

template<typename T> class VirtualListRef {
 private:
  enum Category {
    Single,
    FullArray,
    FullPointerArray,
    RepeatedArray,
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
    struct {
      const T *data;
      uint real_size;
    } repeated_array;
  } m_data;

 public:
  VirtualListRef()
  {
    m_virtual_size = 0;
    m_category = Category::FullArray;
    m_data.single.data = nullptr;
  }

  static VirtualListRef FromSingle(const T *data, uint virtual_size)
  {
    VirtualListRef list;
    list.m_virtual_size = virtual_size;
    list.m_category = Category::Single;
    list.m_data.single.data = data;
    return list;
  }

  static VirtualListRef FromSingle_MaxSize(const T *data)
  {
    return VirtualListRef::FromSingle(data, UINT_MAX);
  }

  static VirtualListRef FromFullArray(const T *data, uint size)
  {
    VirtualListRef list;
    list.m_virtual_size = size;
    list.m_category = Category::FullArray;
    list.m_data.full_array.data = data;
    return list;
  }

  static VirtualListRef FromFullArray(ArrayRef<T> array)
  {
    return VirtualListRef::FromFullArray(array.begin(), array.size());
  }

  static VirtualListRef FromFullPointerArray(const T *const *data, uint size)
  {
    VirtualListRef list;
    list.m_virtual_size = size;
    list.m_category = Category::FullPointerArray;
    list.m_data.full_pointer_array.data = data;
    return list;
  }

  static VirtualListRef FromFullPointerArray(ArrayRef<const T *> data)
  {
    return VirtualListRef::FromFullPointerArray(data.begin(), data.size());
  }

  static VirtualListRef FromRepeatedArray(const T *data, uint real_size, uint virtual_size)
  {
    BLI_assert(virtual_size == 0 || real_size > 0);

    VirtualListRef list;
    list.m_virtual_size = virtual_size;
    list.m_category = Category::RepeatedArray;
    list.m_data.repeated_array.data = data;
    list.m_data.repeated_array.real_size = real_size;
    return list;
  }

  static VirtualListRef FromRepeatedArray(ArrayRef<T> array, uint virtual_size)
  {
    return VirtualListRef::FromRepeatedArray(array.begin(), array.size(), virtual_size);
  }

  const T &operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);
    switch (m_category) {
      case Category::Single:
        return *m_data.single.data;
      case Category::FullArray:
        return m_data.full_array.data[index];
      case Category::FullPointerArray:
        return *m_data.full_pointer_array.data[index];
      case Category::RepeatedArray:
        uint real_index = index % m_data.repeated_array.real_size;
        return m_data.repeated_array.data[real_index];
    }
    BLI_assert(false);
    return *m_data.single.data;
  }

  uint size() const
  {
    return m_virtual_size;
  }
};

}  // namespace BLI

#endif /* __BLI_VIRTUAL_LIST_REF_H__ */
