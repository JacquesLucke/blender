#ifndef __BLI_VIRTUAL_LIST_REF_H__
#define __BLI_VIRTUAL_LIST_REF_H__

#include "BLI_array_ref.h"

namespace BLI {

template<typename T> class VirtualListRef {
 private:
  enum Category {
    Single,
    FullArray,
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
      const T *data;
      uint real_size;
    } repeated_array;
  } m_data;

 public:
  VirtualListRef()
  {
    m_virtual_size = 0;
    m_category = FullArray;
    m_data.single.data = nullptr;
  }

  static VirtualListRef FromSingle(const T *data, uint size)
  {
    VirtualListRef list;
    list.m_virtual_size = size;
    list.m_category = Single;
    list.m_data.single.data = data;
    return list;
  }

  static VirtualListRef FromFullArray(const T *data, uint size)
  {
    VirtualListRef list;
    list.m_virtual_size = size;
    list.m_category = FullArray;
    list.m_data.full_array.data = data;
    return list;
  }

  static VirtualListRef FromFullArray(ArrayRef<T> array)
  {
    return VirtualListRef::FromFullArray(array.begin(), array.size());
  }

  static VirtualListRef FromRepeatedArray(const T *data, uint real_size, uint virtual_size)
  {
    BLI_assert(virtual_size == 0 || real_size > 0);

    VirtualListRef list;
    list.m_virtual_size = virtual_size;
    list.m_category = RepeatedArray;
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
      case Single:
        return *m_data.single.data;
      case FullArray:
        return m_data.full_array.data[index];
      case RepeatedArray:
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

  void materialize(MutableArrayRef<T> dst) const
  {
    BLI_assert(dst.size() == m_virtual_size);
    for (uint i = 0; i < m_virtual_size; i++) {
      dst[i] = (*this)[i];
    }
  }
};

}  // namespace BLI

#endif /* __BLI_VIRTUAL_LIST_REF_H__ */
