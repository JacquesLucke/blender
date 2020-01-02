#ifndef __BLI_INDEX_MASK_H__
#define __BLI_INDEX_MASK_H__

#include "BLI_array_ref.h"
#include "BLI_index_range.h"
#include "BLI_vector.h"
#include "BLI_vector_adaptor.h"

namespace BLI {

class IndexMask {
 private:
  ArrayRef<uint> m_indices;

 public:
  IndexMask() = default;

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

  template<uint N, typename Allocator>
  IndexMask(const Vector<uint, N, Allocator> &vector) : IndexMask(vector.as_ref())
  {
  }

  IndexMask(const VectorAdaptor<uint> &vector) : IndexMask(ArrayRef<uint>(vector))
  {
  }

  explicit IndexMask(uint n) : IndexMask(IndexRange(n))
  {
  }

  operator ArrayRef<uint>() const
  {
    return m_indices;
  }

  const uint *begin() const
  {
    return m_indices.begin();
  }

  const uint *end() const
  {
    return m_indices.end();
  }

  uint operator[](uint index) const
  {
    return m_indices[index];
  }

  uint size() const
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

  IndexRange index_range() const
  {
    return m_indices.index_range();
  }

  uint last() const
  {
    return m_indices.last();
  }
};

}  // namespace BLI

#endif /* __BLI_INDEX_MASK_H__ */
