#include "particles_container.hpp"

namespace BParticles {

ParticlesBlock::ParticlesBlock(ParticlesContainer &container,
                               ArrayRef<float *> float_buffers,
                               ArrayRef<Vec3 *> vec3_buffers,
                               ArrayRef<uint8_t *> byte_buffers,
                               uint active_amount)
    : m_container(container),
      m_float_buffers(float_buffers.to_small_vector()),
      m_vec3_buffers(vec3_buffers.to_small_vector()),
      m_byte_buffers(byte_buffers.to_small_vector()),
      m_active_amount(active_amount)
{
  BLI_assert(m_float_buffers.size() == container.float_attribute_names().size());
  BLI_assert(m_vec3_buffers.size() == container.vec3_attribute_names().size());
  BLI_assert(m_byte_buffers.size() == container.byte_attribute_names().size());
}

ParticlesContainer::ParticlesContainer(uint block_size,
                                       ArrayRef<std::string> float_attribute_names,
                                       ArrayRef<std::string> vec3_attribute_names,
                                       ArrayRef<std::string> byte_attribute_names)
    : m_block_size(block_size),
      m_float_attribute_names(float_attribute_names.to_small_vector()),
      m_vec3_attribute_names(vec3_attribute_names.to_small_vector()),
      m_byte_attribute_names(byte_attribute_names.to_small_vector())
{
  BLI_assert(
      SmallSetVector<std::string>::Disjoint(m_float_attribute_names, m_vec3_attribute_names));
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
  SmallVector<float *> float_buffers;
  for (uint i = 0; i < m_float_attribute_names.size(); i++) {
    float_buffers.append((float *)MEM_malloc_arrayN(m_block_size, sizeof(float), __func__));
  }
  SmallVector<Vec3 *> vec3_buffers;
  for (uint i = 0; i < m_vec3_attribute_names.size(); i++) {
    vec3_buffers.append((Vec3 *)MEM_malloc_arrayN(m_block_size, sizeof(Vec3), __func__));
  }
  SmallVector<uint8_t *> byte_buffers;
  for (uint i = 0; i < m_byte_attribute_names.size(); i++) {
    byte_buffers.append((uint8_t *)MEM_malloc_arrayN(m_block_size, sizeof(uint8_t), __func__));
  }
  ParticlesBlock *block = new ParticlesBlock(*this, float_buffers, vec3_buffers, byte_buffers);
  m_blocks.add_new(block);
  return block;
}

void ParticlesContainer::release_block(ParticlesBlock *block)
{
  BLI_assert(block);
  BLI_assert(block->active_amount() == 0);
  BLI_assert(m_blocks.contains(block));
  BLI_assert(&block->container() == this);

  for (float *buffer : block->float_buffers()) {
    MEM_freeN((void *)buffer);
  }
  for (Vec3 *buffer : block->vec3_buffers()) {
    MEM_freeN((void *)buffer);
  }
  for (uint8_t *buffer : block->byte_buffers()) {
    MEM_freeN((void *)buffer);
  }
  m_blocks.remove(block);
  delete block;
}

template<typename T>
static void move_buffers(ArrayRef<T *> from_buffers,
                         ArrayRef<T *> to_buffers,
                         uint src_start,
                         uint dst_start,
                         uint move_amount)
{
  BLI_assert(from_buffers.size() == to_buffers.size());
  for (uint i = 0; i < from_buffers.size(); i++) {
    memcpy(to_buffers[i] + dst_start, from_buffers[i] + src_start, move_amount * sizeof(T));
  }
}

void ParticlesBlock::MoveUntilFull(ParticlesBlock *from, ParticlesBlock *to)
{
  BLI_assert(&from->container() == &to->container());
  uint move_amount = MIN2(from->active_amount(), to->inactive_amount());
  uint src_start = from->active_amount() - move_amount;
  uint dst_start = to->next_inactive_index();

  if (move_amount == 0) {
    return;
  }

  move_buffers<float>(
      from->float_buffers(), to->float_buffers(), src_start, dst_start, move_amount);
  move_buffers<Vec3>(from->vec3_buffers(), to->vec3_buffers(), src_start, dst_start, move_amount);
  move_buffers<uint8_t>(
      from->byte_buffers(), to->byte_buffers(), src_start, dst_start, move_amount);

  from->active_amount() -= move_amount;
  to->active_amount() += move_amount;
}

void ParticlesBlock::Compress(ArrayRef<ParticlesBlock *> blocks)
{
  std::sort(blocks.begin(), blocks.end(), [](ParticlesBlock *a, ParticlesBlock *b) {
    return a->active_amount() < b->active_amount();
  });

  uint last_non_full = blocks.size() - 1;

  for (uint i = 0; i < blocks.size(); i++) {
    while (i < last_non_full) {
      ParticlesBlock *block = blocks[last_non_full];
      if (block->is_full()) {
        last_non_full--;
        continue;
      }
      ParticlesBlock::MoveUntilFull(blocks[i], block);
      if (blocks[i]->active_amount() == 0) {
        break;
      }
    }
  }
}

}  // namespace BParticles
