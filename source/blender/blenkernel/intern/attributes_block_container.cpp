#include "BKE_attributes_block_container.hpp"

namespace BKE {

AttributesBlockContainer::AttributesBlockContainer(AttributesInfo attributes_info, uint block_size)
    : m_attributes_info(std::move(attributes_info)), m_block_size(block_size)
{
}

AttributesBlockContainer::~AttributesBlockContainer()
{
  for (auto block : m_active_blocks) {
    delete block;
  }
}

uint AttributesBlockContainer::count_active() const
{
  uint amount = 0;
  for (auto &block : m_active_blocks) {
    amount += block->size();
  }
  return amount;
}

void AttributesBlockContainer::update_attributes(AttributesInfo new_info)
{
  AttributesInfoDiff info_diff(m_attributes_info, new_info);
  for (auto &block : m_active_blocks) {
    block->update_buffers(info_diff);
  }
  m_attributes_info = std::move(new_info);
}

AttributesBlock *AttributesBlockContainer::new_block()
{
  AttributesBlock *block = new AttributesBlock(*this, m_block_size);

  {
    std::lock_guard<std::mutex> lock(m_blocks_mutex);
    m_active_blocks.add_new(block);
  }

  return block;
}

void AttributesBlockContainer::release_block(AttributesBlock *block)
{
  {
    std::lock_guard<std::mutex> lock(m_blocks_mutex);
    m_active_blocks.remove(block);
  }
  delete block;
}

void AttributesBlockContainer::flatten_attribute(StringRef attribute_name, void *dst) const
{
  uint attribute_index = m_attributes_info.attribute_index(attribute_name);
  uint element_size = size_of_attribute_type(m_attributes_info.type_of(attribute_index));

  uint offset = 0;
  for (AttributesBlock *block : m_active_blocks) {
    AttributesRef attributes = *block;
    uint size = attributes.size();
    void *src = attributes.get_ptr(attribute_index);
    memcpy(POINTER_OFFSET(dst, offset), src, size * element_size);
    offset += size * element_size;
  }
}

AttributesBlock::AttributesBlock(AttributesBlockContainer &owner, uint capacity)
    : m_owner(owner), m_size(0), m_capacity(capacity)
{
  const AttributesInfo &info = owner.attributes_info();
  m_buffers.reserve(info.size());

  for (AttributeType type : info.types()) {
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
  BLI_assert(to.attributes_info() == from.attributes_info());
  uint move_amount = std::min(from.size(), to.remaining_capacity());

  if (move_amount == 0) {
    return;
  }

  uint src_start = from.m_size - move_amount;
  uint dst_start = to.m_size;

  const AttributesInfo &info = from.attributes_info();

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

void AttributesBlock::update_buffers(const AttributesInfoDiff &info_diff)
{

  Vector<void *> new_buffers(info_diff.new_buffer_amount());
  info_diff.update(m_capacity, m_buffers, new_buffers);
  m_buffers = new_buffers;
}

}  // namespace BKE
