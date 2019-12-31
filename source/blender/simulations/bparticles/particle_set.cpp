#include "particle_set.hpp"

namespace BParticles {

ParticleSet::ParticleSet(const AttributesInfo *attributes_info, uint size)
    : m_attributes_info(attributes_info),
      m_attribute_buffers(attributes_info->size()),
      m_size(size),
      m_capacity(size)
{
  for (uint i : m_attributes_info->indices()) {
    const CPPType &type = m_attributes_info->type_of(i);
    const void *default_value = attributes_info->default_of(i);

    void *buffer = MEM_mallocN_aligned(m_capacity * type.size(), type.alignment(), __func__);
    type.fill_uninitialized(default_value, buffer, m_size);
    m_attribute_buffers[i] = buffer;
  }
}

void ParticleSet::update_attributes(const AttributesInfo *new_attributes_info)
{
  FN::AttributesInfoDiff diff{*m_attributes_info, *new_attributes_info};

  Array<void *> new_buffers(diff.new_buffer_amount());
  diff.update(m_capacity, m_size, m_attribute_buffers, new_buffers);

  m_attribute_buffers = std::move(new_buffers);
  m_attributes_info = new_attributes_info;
}

void ParticleSet::destruct_and_reorder(IndexMask indices_to_destruct)
{
  this->attributes().destruct_and_reorder(indices_to_destruct);
  m_size = m_size - indices_to_destruct.size();
}

void ParticleSet::add_particles(ParticleSet &particles)
{
  BLI_assert(m_attributes_info == particles.m_attributes_info);

  uint required_size = m_size + particles.size();
  if (required_size > m_capacity) {
    this->realloc_particle_attributes(required_size);
  }

  MutableAttributesRef dst{
      *m_attributes_info, m_attribute_buffers, IndexRange(m_size, particles.size())};
  MutableAttributesRef::RelocateUninitialized(particles.attributes(), dst);
}

void ParticleSet::realloc_particle_attributes(uint min_size)
{
  if (min_size <= m_capacity) {
    return;
  }

  uint new_capacity = power_of_2_max_u(min_size);

  for (uint index : m_attributes_info->indices()) {
    const CPPType &type = m_attributes_info->type_of(index);
    void *old_buffer = m_attribute_buffers[index];
    void *new_buffer = MEM_mallocN_aligned(type.size() * new_capacity, type.alignment(), __func__);
    type.relocate_to_uninitialized_n(old_buffer, new_buffer, m_size);
    MEM_freeN(old_buffer);

    m_attribute_buffers[index] = new_buffer;
  }
}

}  // namespace BParticles
