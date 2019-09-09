#pragma once

#include "BKE_attributes_ref.hpp"
#include "BLI_utility_mixins.hpp"

namespace BKE {

class AttributesBlockContainer;

class AttributesBlock : BLI::NonCopyable, BLI::NonMovable {
 private:
  const AttributesInfo *m_attributes_info;
  Vector<void *> m_buffers;
  uint m_size;
  uint m_capacity;
  AttributesBlockContainer *m_owner;

 public:
  AttributesBlock(const AttributesInfo *attributes_info,
                  uint capacity,
                  AttributesBlockContainer &owner);
  ~AttributesBlock();

  void update_buffers(const AttributesInfo *new_info, const AttributesInfoDiff &info_diff);

  uint size() const
  {
    return m_size;
  }

  uint capacity() const
  {
    return m_capacity;
  }

  uint remaining_capacity() const
  {
    return m_capacity - m_size;
  }

  IndexRange active_range() const
  {
    return IndexRange(m_size);
  }

  void set_size(uint new_size)
  {
    BLI_assert(new_size <= m_capacity);
    m_size = new_size;
  }

  AttributesBlockContainer &owner()
  {
    return *m_owner;
  }

  void move(uint old_index, uint new_index);

  static void MoveUntilFull(AttributesBlock &from, AttributesBlock &to);
  static void Compress(MutableArrayRef<AttributesBlock *> blocks);

  AttributesRef as_ref()
  {
    return AttributesRef(*this);
  }

  AttributesRef as_ref__all()
  {
    return AttributesRef(*m_attributes_info, m_buffers, m_capacity);
  }

  operator AttributesRef()
  {
    return AttributesRef(*m_attributes_info, m_buffers, m_size);
  }
};

}  // namespace BKE
