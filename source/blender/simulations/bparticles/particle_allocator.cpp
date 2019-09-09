#include "particle_allocator.hpp"

namespace BParticles {

ParticleAllocator::ParticleAllocator(ParticlesState &state) : m_state(state)
{
}

AttributesBlock &ParticleAllocator::get_non_full_block(AttributesBlockContainer &container)
{
  AttributesBlock *cached_block = m_non_full_cache.lookup_default(&container, nullptr);
  if (cached_block != nullptr) {
    if (cached_block->remaining_capacity() > 0) {
      return *cached_block;
    }

    m_non_full_cache.remove(&container);
  }

  AttributesBlock *block = container.new_block();
  m_non_full_cache.add_new(&container, block);
  m_allocated_blocks.append(block);
  return *block;
}

void ParticleAllocator::allocate_block_ranges(StringRef particle_type_name,
                                              uint size,
                                              Vector<AttributesBlock *> &r_blocks,
                                              Vector<IndexRange> &r_ranges)
{
  AttributesBlockContainer &container = m_state.particle_container(particle_type_name);

  uint remaining_size = size;
  while (remaining_size > 0) {
    AttributesBlock &block = this->get_non_full_block(container);

    uint size_to_use = std::min(block.remaining_capacity(), remaining_size);
    IndexRange range(block.size(), size_to_use);
    block.set_size(block.size() + size_to_use);

    r_blocks.append(&block);
    r_ranges.append(range);

    this->initialize_new_particles(block, container, range);

    remaining_size -= size_to_use;
  }
}

void ParticleAllocator::initialize_new_particles(AttributesBlock &block,
                                                 AttributesBlockContainer &container,
                                                 IndexRange pindices)
{
  AttributesRef attributes = block.as_ref().slice(pindices);
  for (uint i : attributes.info().attribute_indices()) {
    attributes.init_default(i);
  }

  MutableArrayRef<int32_t> particle_ids = block.as_ref__all().get<int32_t>("ID");
  IndexRange new_ids = container.new_ids(pindices.size());
  for (uint i = 0; i < pindices.size(); i++) {
    uint pindex = pindices[i];
    particle_ids[pindex] = new_ids[i];
  }
}

const AttributesInfo &ParticleAllocator::attributes_info(StringRef particle_type_name)
{
  return m_state.particle_container(particle_type_name).attributes_info();
}

AttributesRefGroup ParticleAllocator::request(StringRef particle_type_name, uint size)
{
  Vector<AttributesBlock *> blocks;
  Vector<IndexRange> ranges;
  this->allocate_block_ranges(particle_type_name, size, blocks, ranges);

  const AttributesInfo &attributes_info = this->attributes_info(particle_type_name);

  Vector<ArrayRef<void *>> buffers;
  for (uint i = 0; i < blocks.size(); i++) {
    buffers.append(blocks[i]->as_ref().buffers());
  }

  return AttributesRefGroup(attributes_info, std::move(buffers), std::move(ranges));
}

}  // namespace BParticles
