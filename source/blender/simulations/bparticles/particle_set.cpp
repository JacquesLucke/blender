#include "particle_set.hpp"

namespace BParticles {

ParticleSet::ParticleSet(const AttributesInfoBuilder &attributes_info_builder, uint size)
    : m_attributes_info(BLI::make_unique<AttributesInfo>(attributes_info_builder)),
      m_size(size),
      m_capacity(size)
{
}

void ParticleSet::update_attributes(const AttributesInfoBuilder &new_attributes_info_builder)
{
  auto new_attributes_info = BLI::make_unique<AttributesInfo>(new_attributes_info_builder);
  FN::AttributesInfoDiff diff{*m_attributes_info, *new_attributes_info};

  Vector<void *> new_buffers(diff.new_buffer_amount());
  diff.update(m_capacity, m_size, m_attribute_buffers, new_buffers);

  m_attribute_buffers = std::move(new_buffers);
  m_attributes_info = std::move(new_attributes_info);
}

void ParticleSet::destruct_and_reorder(IndexMask indices_to_destruct)
{
  this->attributes().destruct_and_reorder(indices_to_destruct);
  m_size = m_size - indices_to_destruct.size();
}

void ParticleSet::add_particles(ParticleSet &particles)
{
  /* TODO */
}

}  // namespace BParticles
