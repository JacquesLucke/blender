#ifndef __FN_MULTI_FUNCTION_MASK_H__
#define __FN_MULTI_FUNCTION_MASK_H__

#include "BLI_index_range.h"
#include "BLI_array_ref.h"

namespace FN {

using BLI::ArrayRef;
using BLI::IndexRange;

class MFMask {
 private:
  ArrayRef<uint> m_indices;

 public:
  MFMask(ArrayRef<uint> indices) : m_indices(indices)
  {
#ifdef DEBUG
    for (uint i = 1; i < indices.size(); i++) {
      BLI_assert(indices[i - 1] < indices[i]);
    }
#endif
  }

  MFMask(IndexRange range) : m_indices(range.as_array_ref())
  {
  }

  MFMask(const std::initializer_list<uint> &list) : MFMask(ArrayRef<uint>(list))
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

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_MASK_H__ */
