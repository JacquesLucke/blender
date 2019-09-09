#pragma once

#include <mutex>
#include <atomic>

#include "BKE_attributes_block.hpp"

namespace BKE {

class AttributesBlockContainer : BLI::NonCopyable, BLI::NonMovable {
 private:
  std::unique_ptr<AttributesInfo> m_attributes_info;
  uint m_block_size;
  SetVector<AttributesBlock *> m_active_blocks;
  std::mutex m_blocks_mutex;
  std::atomic<uint> m_next_id;

 public:
  AttributesBlockContainer(std::unique_ptr<AttributesInfo> attributes_info, uint block_size);
  ~AttributesBlockContainer();

  uint count_active() const;

  const AttributesInfo &attributes_info() const
  {
    return *m_attributes_info;
  }

  void update_attributes(std::unique_ptr<AttributesInfo> new_info);

  AttributesBlock *new_block();
  void release_block(AttributesBlock *block);

  void flatten_attribute(StringRef attribute_name, void *dst) const;

  template<typename T> Vector<T> flatten_attribute(StringRef attribute_name)
  {
    BLI_assert(m_attributes_info->type_of(attribute_name) == attribute_type_by_type<T>::value);
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

}  // namespace BKE
