#pragma once

#include <mutex>
#include <atomic>

#include "BLI_utility_mixins.h"

#include "BKE_attributes_ref.hpp"

namespace BKE {

class AttributesBlock;

class AttributesBlockContainer : BLI::NonCopyable, BLI::NonMovable {
 private:
  AttributesInfo m_attributes_info;
  uint m_block_size;
  SetVector<AttributesBlock *> m_active_blocks;
  std::mutex m_blocks_mutex;
  std::atomic<uint> m_next_id;

 public:
  AttributesBlockContainer(AttributesInfo attributes_info, uint block_size);
  ~AttributesBlockContainer();

  uint count_active() const;

  const AttributesInfo &attributes_info() const
  {
    return m_attributes_info;
  }

  void update_attributes(AttributesInfo new_info);

  AttributesBlock *new_block();
  void release_block(AttributesBlock *block);

  void flatten_attribute(StringRef attribute_name, void *dst) const;

  template<typename T> Vector<T> flatten_attribute(StringRef attribute_name)
  {
    BLI_assert(m_attributes_info.type_of(attribute_name) == attribute_type_by_type<T>::value);
    Vector<T> result(this->count_active());
    this->flatten_attribute(attribute_name, (void *)result.begin());
    return result;
  }

  friend bool operator==(const AttributesBlockContainer &a, const AttributesBlockContainer &b)
  {
    return &a == &b;
  }

  IndexRange new_ids(uint amount)
  {
    uint start = m_next_id.fetch_add(amount);
    return IndexRange(start, amount);
  }

  ArrayRef<AttributesBlock *> active_blocks()
  {
    return m_active_blocks;
  }
};

class AttributesBlock : BLI::NonCopyable, BLI::NonMovable {
 private:
  AttributesBlockContainer &m_owner;
  Vector<void *> m_buffers;
  uint m_size;
  uint m_capacity;

 public:
  AttributesBlock(AttributesBlockContainer &owner, uint capacity);
  ~AttributesBlock();

  void update_buffers(const AttributesInfoDiff &info_diff);

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

  const AttributesInfo &attributes_info() const
  {
    return m_owner.attributes_info();
  }

  AttributesBlockContainer &owner()
  {
    return m_owner;
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
    return AttributesRef(m_owner.attributes_info(), m_buffers, m_capacity);
  }

  operator AttributesRef()
  {
    return AttributesRef(m_owner.attributes_info(), m_buffers, m_size);
  }
};

}  // namespace BKE
