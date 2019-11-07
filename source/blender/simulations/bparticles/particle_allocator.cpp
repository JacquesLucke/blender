#include "particle_allocator.hpp"

namespace BParticles {

ParticleAllocator::ParticleAllocator(
    ParticlesState &state, const StringMap<const AttributesDefaults *> &attribute_defaults)
    : m_state(state), m_attributes_defaults(attribute_defaults)
{
}

void ParticleAllocator::allocate_buffer_ranges(AttributesBlockContainer &container,
                                               uint size,
                                               Vector<ArrayRef<void *>> &r_buffers,
                                               Vector<IndexRange> &r_ranges)
{
  std::lock_guard<std::mutex> lock(m_request_mutex);

  uint remaining_size = size;
  while (remaining_size > 0) {
    AttributesBlock *cached_block = m_non_full_cache.lookup_default(&container, nullptr);
    if (cached_block != nullptr) {
      uint remaining_in_block = cached_block->unused_capacity();
      BLI_assert(remaining_in_block > 0);
      uint size_to_use = std::min(remaining_size, remaining_in_block);

      IndexRange range(cached_block->used_size(), size_to_use);
      r_buffers.append(cached_block->buffers());
      r_ranges.append(range);
      remaining_size -= size_to_use;

      cached_block->set_used_size(range.one_after_last());
      if (cached_block->unused_capacity() == 0) {
        m_non_full_cache.remove(&container);
      }
      continue;
    }
    else {
      AttributesBlock &new_block = container.new_block();
      m_non_full_cache.add_new(&container, &new_block);
      m_allocated_blocks.append(&new_block);
    }
  }
}

void ParticleAllocator::initialize_new_particles(StringRef name,
                                                 AttributesRefGroup &attributes_group)
{
  const AttributesDefaults &defaults = *m_attributes_defaults.lookup(name);

  for (AttributesRef attributes : attributes_group) {
    for (uint i : attributes.info().indices()) {
      StringRef attribute_name = attributes.info().name_of(i);
      const FN::CPPType &attribute_type = attributes.info().type_of(i);
      const void *default_value = defaults.get(attribute_name, attribute_type);
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
  AttributesBlockContainer &container = m_state.particle_container(particle_system_name);

  Vector<ArrayRef<void *>> buffers;
  Vector<IndexRange> ranges;
  this->allocate_buffer_ranges(container, size, buffers, ranges);

  const AttributesInfo &attributes_info = container.info();
  AttributesRefGroup attributes_group(attributes_info, std::move(buffers), std::move(ranges));

  this->initialize_new_particles(particle_system_name, attributes_group);

  return attributes_group;
}

}  // namespace BParticles
