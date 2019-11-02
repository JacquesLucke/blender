#include "FN_attributes_block_container.h"

namespace FN {

AttributesBlockContainer::AttributesBlockContainer(AttributesInfo attributes_info, uint block_size)
    : m_attributes_info(std::move(attributes_info)), m_block_size(block_size)
{
}

AttributesBlockContainer::~AttributesBlockContainer()
{
  while (m_active_blocks.size() > 0) {
    this->release_block(**m_active_blocks.begin());
  }
}

AttributesBlock &AttributesBlockContainer::new_block()
{
  AttributesBlock *block = new AttributesBlock(*this);
  {
    std::lock_guard<std::mutex> lock(m_blocks_mutex);
    m_active_blocks.add(block);
  }
  return *block;
}

void AttributesBlockContainer::release_block(AttributesBlock &block)
{
  {
    std::lock_guard<std::mutex> lock(m_blocks_mutex);
    m_active_blocks.remove(&block);
  }
  delete &block;
}

AttributesBlock::AttributesBlock(AttributesBlockContainer &owner) : m_owner(owner), m_used_size(0)
{
  for (const CPPType *type : owner.info().types()) {
    void *buffer = MEM_malloc_arrayN(owner.block_size(), type->size(), __func__);
    m_buffers.append(buffer);
  }
}

AttributesBlock::~AttributesBlock()
{
  for (uint attribute_index : m_owner.info().indices()) {
    const CPPType &type = m_owner.info().type_of(attribute_index);
    type.destruct_n(m_buffers[attribute_index], m_used_size);
  }
}

}  // namespace FN
