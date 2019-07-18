#include "particle_allocator.hpp"

namespace BParticles {

ParticleAllocator::ParticleAllocator(ParticlesState &state) : m_state(state)
{
}

ParticlesBlock &ParticleAllocator::get_non_full_block(StringRef particle_type_name)
{
  ParticlesContainer &container = m_state.particle_container(particle_type_name);

  uint index = 0;
  while (index < m_non_full_cache.size()) {
    if (m_non_full_cache[index]->unused_amount() == 0) {
      m_non_full_cache.remove_and_reorder(index);
      continue;
    }

    if (m_non_full_cache[index]->container() == container) {
      return *m_non_full_cache[index];
    }
    index++;
  }

  ParticlesBlock &block = container.new_block();
  m_non_full_cache.append(&block);
  m_allocated_blocks.append(&block);
  return block;
}

void ParticleAllocator::allocate_block_ranges(StringRef particle_type_name,
                                              uint size,
                                              SmallVector<ParticlesBlock *> &r_blocks,
                                              SmallVector<Range<uint>> &r_ranges)
{
  uint remaining_size = size;
  while (remaining_size > 0) {
    ParticlesBlock &block = this->get_non_full_block(particle_type_name);

    uint size_to_use = std::min(block.unused_amount(), remaining_size);
    Range<uint> range(block.active_amount(), block.active_amount() + size_to_use);
    block.active_amount() += size_to_use;

    r_blocks.append(&block);
    r_ranges.append(range);

    AttributeArrays attributes = block.attributes_slice(range);
    for (uint i : attributes.info().attribute_indices()) {
      attributes.init_default(i);
    }

    remaining_size -= size_to_use;
  }
}

AttributesInfo &ParticleAllocator::attributes_info(StringRef particle_type_name)
{
  return m_state.particle_container(particle_type_name).attributes_info();
}

ParticleSets ParticleAllocator::request(StringRef particle_type_name, uint size)
{
  SmallVector<ParticlesBlock *> blocks;
  SmallVector<Range<uint>> ranges;
  this->allocate_block_ranges(particle_type_name, size, blocks, ranges);

  AttributesInfo &attributes_info = this->attributes_info(particle_type_name);

  SmallVector<ParticleSet> sets;
  for (uint i = 0; i < blocks.size(); i++) {
    sets.append(ParticleSet(*blocks[i], ranges[i].as_array_ref()));
  }

  return ParticleSets(particle_type_name, attributes_info, sets);
}

}  // namespace BParticles
