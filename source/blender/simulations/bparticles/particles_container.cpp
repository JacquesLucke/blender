#include "particles_container.hpp"

namespace BParticles {

ParticlesBlock::ParticlesBlock(ParticlesContainer &container,
                               ArrayRef<float *> float_buffers,
                               ArrayRef<Vec3 *> vec3_buffers,
                               uint active_amount)
    : m_container(container),
      m_float_buffers(float_buffers.to_small_vector()),
      m_vec3_buffers(vec3_buffers.to_small_vector()),
      m_active_amount(active_amount)
{
  BLI_assert(m_float_buffers.size() == container.float_attribute_amount());
  BLI_assert(m_vec3_buffers.size() == container.vec3_attribute_amount());
}

ParticlesContainer::ParticlesContainer(uint block_size,
                                       const SmallVector<std::string> &float_attribute_names,
                                       const SmallVector<std::string> &vec3_attribute_names)
    : m_block_size(block_size),
      m_float_attribute_names(float_attribute_names),
      m_vec3_attribute_names(vec3_attribute_names)
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
  ParticlesBlock *block = new ParticlesBlock(*this, float_buffers, vec3_buffers);
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
  m_blocks.remove(block);
  delete block;
}

}  // namespace BParticles
