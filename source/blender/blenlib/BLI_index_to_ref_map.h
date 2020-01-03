#ifndef __BLI_INDEX_TO_REF_MAP_H__
#define __BLI_INDEX_TO_REF_MAP_H__

#include "BLI_array_cxx.h"

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

}  // namespace BLI

#endif /* __BLI_INDEX_TO_REF_MAP_H__ */
