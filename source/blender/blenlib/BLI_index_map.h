#ifndef __BLI_INDEX_MAP_H__
#define __BLI_INDEX_MAP_H__

#include "BLI_array_cxx.h"

namespace BLI {

template<typename ValueT, uint N = 4, typename Allocator = GuardedAllocator> class IndexMap {
 private:
  Array<ValueT, N, Allocator> m_array;
  ValueT m_sentinel;

 public:
  IndexMap(uint size, ValueT sentinel) : m_array(size, sentinel), m_sentinel(sentinel)
  {
  }

  uint size() const
  {
    return m_array.size();
  }

  void add(uint key, const ValueT &value)
  {
    m_array[key] = value;
  }

  void add_new(uint key, const ValueT &value)
  {
    BLI_assert(m_array[key] == m_sentinel);
    m_array[key] = value;
  }

  bool contains(uint key) const
  {
    return m_array[key] != m_sentinel;
  }

  const ValueT &lookup(uint key) const
  {
    BLI_assert(this->contains(key));
    return m_array[key];
  }
};

}  // namespace BLI

#endif /* __BLI_INDEX_MAP_H__ */
