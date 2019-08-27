#include "particle_set.hpp"

namespace BParticles {

ParticleSets::ParticleSets(StringRef particle_type_name,
                           AttributesInfo &attributes_info,
                           ArrayRef<ParticleSet> sets)
    : m_particle_type_name(particle_type_name), m_attributes_info(attributes_info), m_sets(sets)
{
  m_size = 0;
  for (auto &set : sets) {
    m_size += set.size();
  }
}

void ParticleSets::set_elements(uint index, void *data)
{
  AttributeType type = m_attributes_info.type_of(index);
  uint element_size = size_of_attribute_type(type);

  void *remaining_data = data;

  for (ParticleSet particles : m_sets) {
    AttributeArrays attributes = particles.attributes();
    void *dst = attributes.get_ptr(index);

    for (uint i = 0; i < particles.size(); i++) {
      uint pindex = particles.pindices()[i];
      memcpy(POINTER_OFFSET(dst, element_size * pindex),
             POINTER_OFFSET(remaining_data, element_size * i),
             element_size);
    }

    remaining_data = POINTER_OFFSET(remaining_data, particles.size() * element_size);
  }
}

void ParticleSets::set_repeated_elements(uint index,
                                         void *data,
                                         uint data_element_amount,
                                         void *default_value)
{
  if (data_element_amount == 0) {
    this->fill_elements(index, default_value);
    return;
  }

  AttributeType type = m_attributes_info.type_of(index);
  uint element_size = size_of_attribute_type(type);
  uint offset = 0;
  for (ParticleSet particles : m_sets) {
    AttributeArrays attributes = particles.attributes();
    void *dst = attributes.get_ptr(index);
    for (uint pindex : particles.pindices()) {
      memcpy(POINTER_OFFSET(dst, element_size * pindex),
             POINTER_OFFSET(data, element_size * offset),
             element_size);
      offset++;
      if (offset == data_element_amount) {
        offset = 0;
      }
    }
  }
}

void ParticleSets::fill_elements(uint index, void *value)
{
  AttributeType type = m_attributes_info.type_of(index);
  uint element_size = size_of_attribute_type(type);

  for (ParticleSet particles : m_sets) {
    AttributeArrays attributes = particles.attributes();
    void *dst = attributes.get_ptr(index);

    for (uint pindex : particles.pindices()) {
      memcpy(POINTER_OFFSET(dst, element_size * pindex), value, element_size);
    }
  }
}

}  // namespace BParticles
