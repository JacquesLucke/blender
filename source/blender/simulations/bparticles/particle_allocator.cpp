#include "particle_allocator.hpp"

namespace BParticles {

ParticleAllocator::ParticleAllocator(ParticlesState &state) : m_state(state)
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
      uint remaining_in_block = cached_block->remaining_capacity();
      BLI_assert(remaining_in_block > 0);
      uint size_to_use = std::min(remaining_size, remaining_in_block);

      IndexRange range(cached_block->size(), size_to_use);
      r_buffers.append(cached_block->as_ref__all().buffers());
      r_ranges.append(range);
      remaining_size -= size_to_use;

      cached_block->set_size(range.one_after_last());
      if (cached_block->remaining_capacity() == 0) {
        m_non_full_cache.remove(&container);
      }
      continue;
    }
    else {
      AttributesBlock *new_block = container.new_block();
      m_non_full_cache.add_new(&container, new_block);
      m_allocated_blocks.append(new_block);
    }
  }
}

void ParticleAllocator::initialize_new_particles(AttributesBlockContainer &container,
                                                 AttributesRefGroup &attributes_group)
{
  for (AttributesRef attributes : attributes_group) {
    for (uint i : attributes.info().attribute_indices()) {
      attributes.init_default(i);
    }

    MutableArrayRef<int32_t> particle_ids = attributes.get<int32_t>("ID");
    IndexRange new_ids = container.new_ids(attributes.size());
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

  const AttributesInfo &attributes_info = container.attributes_info();
  AttributesRefGroup attributes_group(attributes_info, std::move(buffers), std::move(ranges));

  this->initialize_new_particles(container, attributes_group);

  return attributes_group;
}

}  // namespace BParticles
