#include "BKE_attributes_block.hpp"

namespace BKE {

AttributesBlock::AttributesBlock(const AttributesInfo *attributes_info,
                                 uint capacity,
                                 AttributesBlockContainer &owner)
    : m_attributes_info(attributes_info), m_size(0), m_capacity(capacity), m_owner(&owner)
{
  BLI_assert(attributes_info != nullptr);
  m_buffers.reserve(attributes_info->size());

  for (AttributeType type : m_attributes_info->types()) {
    uint byte_size = capacity * size_of_attribute_type(type);
    void *ptr = MEM_mallocN_aligned(byte_size, 64, __func__);
    m_buffers.append(ptr);
  }
}

AttributesBlock::~AttributesBlock()
{
  for (void *ptr : m_buffers) {
    MEM_freeN(ptr);
  }
}

void AttributesBlock::move(uint old_index, uint new_index)
{
  AttributesRef attributes = *this;

  for (uint attribute_index : attributes.info().attribute_indices()) {
    void *ptr = attributes.get_ptr(attribute_index);
    uint size = attributes.attribute_size(attribute_index);
    void *src = POINTER_OFFSET(ptr, old_index * size);
    void *dst = POINTER_OFFSET(ptr, new_index * size);
    memcpy(dst, src, size);
  }
}

void AttributesBlock::MoveUntilFull(AttributesBlock &from, AttributesBlock &to)
{
  BLI_assert(to.m_attributes_info == from.m_attributes_info);
  uint move_amount = std::min(from.size(), to.remaining_capacity());

  if (move_amount == 0) {
    return;
  }

  uint src_start = from.m_size - move_amount;
  uint dst_start = to.m_size;

  const AttributesInfo &info = *from.m_attributes_info;

  for (uint i = 0; i < info.size(); i++) {
    void *from_buffer = from.m_buffers[i];
    void *to_buffer = to.m_buffers[i];
    AttributeType type = info.type_of(i);
    uint size = size_of_attribute_type(type);
    memcpy((char *)to_buffer + size * dst_start,
           (char *)from_buffer + size * src_start,
           size * move_amount);
  }

  from.m_size -= move_amount;
  to.m_size += move_amount;
}

void AttributesBlock::Compress(MutableArrayRef<AttributesBlock *> blocks)
{
  std::sort(blocks.begin(), blocks.end(), [](AttributesBlock *a, AttributesBlock *b) {
    return a->size() < b->size();
  });

  uint last_non_full = blocks.size() - 1;

  for (uint i = 0; i < blocks.size(); i++) {
    while (i < last_non_full) {
      AttributesBlock &block = *blocks[last_non_full];
      if (block.m_size == block.m_capacity) {
        last_non_full--;
        continue;
      }

      AttributesBlock::MoveUntilFull(*blocks[i], block);
      if (blocks[i]->size() == 0) {
        break;
      }
    }
  }
}

void AttributesBlock::update_buffers(const AttributesInfo *new_info,
                                     const AttributesInfoDiff &info_diff)
{
  m_attributes_info = new_info;

  Vector<void *> new_buffers(new_info->size());
  info_diff.update(m_capacity, m_buffers, new_buffers);
  m_buffers = new_buffers;
}

}  // namespace BKE
