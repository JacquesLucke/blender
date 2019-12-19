#ifndef __BLI_INDEX_MASK_H__
#define __BLI_INDEX_MASK_H__

#include "BLI_array_ref.h"
#include "BLI_index_range.h"

namespace BLI {

class IndexMask {
 private:
  ArrayRef<uint> m_indices;

 public:
  IndexMask(ArrayRef<uint> indices) : m_indices(indices)
  {
#ifdef DEBUG
    for (uint i = 1; i < indices.size(); i++) {
      BLI_assert(indices[i - 1] < indices[i]);
    }
#endif
  }

  IndexMask(IndexRange range) : m_indices(range.as_array_ref())
  {
  }

  IndexMask(const std::initializer_list<uint> &list) : IndexMask(ArrayRef<uint>(list))
  {
  }

  uint indices_amount() const
  {
    return m_indices.size();
  }

  uint min_array_size() const
  {
    return (m_indices.size() == 0) ? 0 : m_indices.last() + 1;
  }

  ArrayRef<uint> indices() const
  {
    return m_indices;
  }

  bool is_range() const
  {
    return m_indices.size() > 0 && m_indices.last() - m_indices.first() == m_indices.size() - 1;
  }

  IndexRange as_range() const
  {
    BLI_assert(this->is_range());
    return IndexRange{m_indices.first(), m_indices.size()};
  }

  template<typename FuncT> void foreach_index(const FuncT &func) const
  {
    if (this->is_range()) {
      IndexRange range = this->as_range();
      for (uint i : range) {
        func(i);
      }
    }
    else {
      for (uint i : m_indices) {
        func(i);
      }
    }
  }
};

}  // namespace BLI

#endif /* __BLI_INDEX_MASK_H__ */
