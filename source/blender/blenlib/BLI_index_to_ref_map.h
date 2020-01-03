#ifndef __BLI_INDEX_TO_REF_MAP_H__
#define __BLI_INDEX_TO_REF_MAP_H__

#include "BLI_array_cxx.h"
#include "BLI_multi_map.h"

namespace BLI {

template<typename T, uint N = 4, typename Allocator = GuardedAllocator> class IndexToRefMap {
 private:
  Array<T *, N, Allocator> m_array;

 public:
  IndexToRefMap(uint size) : m_array(size, nullptr)
  {
  }

  uint size() const
  {
    return m_array.size();
  }

  void add(uint key, T &value)
  {
    m_array[key] = &value;
  }

  void add_new(uint key, T &value)
  {
    BLI_assert(m_array[key] == nullptr);
    m_array[key] = &value;
  }

  bool contains(uint key) const
  {
    return m_array[key] != nullptr;
  }

  const T &lookup(uint key) const
  {
    BLI_assert(this->contains(key));
    return *m_array[key];
  }

  T &lookup(uint key)
  {
    BLI_assert(this->contains(key));
    return *m_array[key];
  }
};

#define IndexToRefMultiMap_UNMAPPED nullptr
#define IndexToRefMultiMap_MULTIMAPPED ((T *)1)

template<typename T, uint N = 4, typename Allocator = GuardedAllocator> class IndexToRefMultiMap {
 private:
  Array<T *> m_array;
  MultiMap<uint, T *> m_fallback_multimap;

 public:
  IndexToRefMultiMap(uint size) : m_array(size, IndexToRefMultiMap_UNMAPPED)
  {
  }

  bool contains(uint key) const
  {
    return m_array[key] != IndexToRefMultiMap_UNMAPPED;
  }

  ArrayRef<T *> lookup(uint key) const
  {
    T *const *stored_value_addr = &m_array[key];
    const T *stored_value = *stored_value_addr;
    if (stored_value == IndexToRefMultiMap_UNMAPPED) {
      return {};
    }
    else if (stored_value == IndexToRefMultiMap_MULTIMAPPED) {
      return m_fallback_multimap.lookup(key);
    }
    else {
      return ArrayRef<T *>(stored_value_addr, 1);
    }
  }

  T &lookup_single(uint key)
  {
    T *stored_value = m_array[key];
    BLI_assert(stored_value != IndexToRefMultiMap_UNMAPPED &&
               stored_value != IndexToRefMultiMap_MULTIMAPPED);
    return *stored_value;
  }

  void add(uint key, T &value)
  {
    T **stored_value_addr = &m_array[key];
    T *stored_value = *stored_value_addr;
    if (stored_value == IndexToRefMultiMap_UNMAPPED) {
      *stored_value_addr = &value;
    }
    else if (stored_value == IndexToRefMultiMap_MULTIMAPPED) {
      m_fallback_multimap.add(key, &value);
    }
    else {
      T *other_value = stored_value;
      *stored_value_addr = IndexToRefMultiMap_MULTIMAPPED;
      m_fallback_multimap.add_multiple_new(key, {other_value, &value});
    }
  }
};

#undef IndexToRefMultiMap_UNMAPPED
#undef IndexToRefMultiMap_MULTIMAPPED

}  // namespace BLI

#endif /* __BLI_INDEX_TO_REF_MAP_H__ */
