#include "particle_allocator.hpp"

namespace BParticles {

ParticleAllocator::ParticleAllocator(ParticlesState &state) : m_state(state)
{
}

void ParticleAllocator::initialize_new_particles(AttributesRefGroup &attributes_group)
{
  const AttributesInfo &info = attributes_group.info();

  for (MutableAttributesRef attributes : attributes_group) {
    for (uint i : info.indices()) {
      StringRef attribute_name = info.name_of(i);
      const void *default_value = info.default_of(attribute_name);
      attributes.get(i).fill__uninitialized(default_value);
    }

    MutableArrayRef<int32_t> particle_ids = attributes.get<int32_t>("ID");
    IndexRange new_ids = m_state.get_new_particle_ids(attributes.size());
    BLI_assert(particle_ids.size() == new_ids.size());
    for (uint i = 0; i < new_ids.size(); i++) {
      particle_ids[i] = new_ids[i];
    }
  }
}

AttributesRefGroup ParticleAllocator::request(StringRef particle_system_name, uint size)
{
  ParticleSet &main_set = m_state.particle_container(particle_system_name);
  const AttributesInfo &attributes_info = main_set.attributes_info();

  ParticleSet *particles = new ParticleSet(attributes_info, false);
  particles->reserve(size);
  particles->increase_size_without_realloc(size);
  MutableAttributesRef attributes = particles->attributes();

  {
    std::lock_guard<std::mutex> lock(m_request_mutex);
    m_allocated_particles.add(particle_system_name, particles);
  }

  Vector<ArrayRef<void *>> buffers;
  Vector<IndexRange> ranges;
  buffers.append(attributes.internal_buffers());
  ranges.append(attributes.internal_range());

  AttributesRefGroup attributes_group(attributes_info, std::move(buffers), std::move(ranges));

  this->initialize_new_particles(attributes_group);

  return attributes_group;
}

}  // namespace BParticles
