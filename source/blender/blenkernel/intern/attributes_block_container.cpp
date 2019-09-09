#include "BKE_attributes_block_container.hpp"

namespace BKE {

AttributesBlockContainer::AttributesBlockContainer(std::unique_ptr<AttributesInfo> attributes_info,
                                                   uint block_size)
    : m_attributes_info(std::move(attributes_info)), m_block_size(block_size)
{
  BLI_assert(m_attributes_info.get() != nullptr);
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

void AttributesBlockContainer::update_attributes(std::unique_ptr<AttributesInfo> new_info)
{
  AttributesInfoDiff info_diff(*m_attributes_info, *new_info);
  for (auto &block : m_active_blocks) {
    block->update_buffers(new_info.get(), info_diff);
  }
  m_attributes_info = std::move(new_info);
}

AttributesBlock *AttributesBlockContainer::new_block()
{
  AttributesBlock *block = new AttributesBlock(m_attributes_info.get(), m_block_size, *this);

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
  uint attribute_index = m_attributes_info->attribute_index(attribute_name);
  uint element_size = size_of_attribute_type(m_attributes_info->type_of(attribute_index));

  uint offset = 0;
  for (AttributesBlock *block : m_active_blocks) {
    AttributesRef attributes = *block;
    uint size = attributes.size();
    void *src = attributes.get_ptr(attribute_index);
    memcpy(POINTER_OFFSET(dst, offset), src, size * element_size);
    offset += size * element_size;
  }
}

}  // namespace BKE
