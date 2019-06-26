#include "particles_container.hpp"

namespace BParticles {

ParticlesBlock::ParticlesBlock(ParticlesContainer &container, AttributeArraysCore &attributes_core)
    : m_container(container), m_attributes_core(attributes_core)
{
}

ParticlesContainer::ParticlesContainer(AttributesInfo attributes, uint block_size)
    : m_attributes_info(std::move(attributes)), m_block_size(block_size)
{
}

ParticlesContainer::~ParticlesContainer()
{
  while (m_blocks.size() > 0) {
    ParticlesBlock *block = m_blocks.any();
    block->clear();
    this->release_block(block);
  }
}

ParticlesBlock *ParticlesContainer::new_block()
{
  AttributeArraysCore attributes_core = AttributeArraysCore::NewWithSeparateAllocations(
      m_attributes_info, m_block_size);
  ParticlesBlock *block = new ParticlesBlock(*this, attributes_core);
  m_blocks.add_new(block);
  return block;
}

void ParticlesContainer::release_block(ParticlesBlock *block)
{
  BLI_assert(block);
  BLI_assert(block->active_amount() == 0);
  BLI_assert(m_blocks.contains(block));
  BLI_assert(&block->container() == this);

  block->attributes_core().free_buffers();
  m_blocks.remove(block);
  delete block;
}

void ParticlesContainer::update_attributes(AttributesInfo new_info)
{
  m_attributes_info = new_info;
  /* TODO: actually update attributes */
}

void ParticlesBlock::MoveUntilFull(ParticlesBlock &from, ParticlesBlock &to)
{
  BLI_assert(&from.container() == &to.container());
  uint move_amount = MIN2(from.active_amount(), to.inactive_amount());
  uint src_start = from.active_amount() - move_amount;
  uint dst_start = to.next_inactive_index();

  if (move_amount == 0) {
    return;
  }

  uint attribute_amount = from.container().attributes_info().amount();
  for (uint i = 0; i < attribute_amount; i++) {
    void *from_buffer = from.attributes_core().get_ptr(i);
    void *to_buffer = to.attributes_core().get_ptr(i);
    AttributeType type = from.attributes_core().get_type(i);
    uint size = size_of_attribute_type(type);
    memcpy((char *)to_buffer + size * dst_start,
           (char *)from_buffer + size * src_start,
           size * move_amount);
  }

  from.active_amount() -= move_amount;
  to.active_amount() += move_amount;
}

void ParticlesBlock::Compress(ArrayRef<ParticlesBlock *> blocks)
{
  std::sort(blocks.begin(), blocks.end(), [](ParticlesBlock *a, ParticlesBlock *b) {
    return a->active_amount() < b->active_amount();
  });

  uint last_non_full = blocks.size() - 1;

  for (uint i = 0; i < blocks.size(); i++) {
    while (i < last_non_full) {
      ParticlesBlock &block = *blocks[last_non_full];
      if (block.is_full()) {
        last_non_full--;
        continue;
      }
      ParticlesBlock::MoveUntilFull(*blocks[i], block);
      if (blocks[i]->active_amount() == 0) {
        break;
      }
    }
  }
}

}  // namespace BParticles
