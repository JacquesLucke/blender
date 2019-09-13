#pragma once

#include "particles_state.hpp"

namespace BParticles {

using BKE::AttributesRefGroup;

/**
 * This class allows allocating new blocks from different particle containers.
 * A single instance is not thread safe, but multiple allocator instances can
 * be used by multiple threads at the same time.
 * It might hand out the same block more than once until it is full.
 */
class ParticleAllocator {
 private:
  ParticlesState &m_state;
  Map<AttributesBlockContainer *, AttributesBlock *> m_non_full_cache;
  Vector<AttributesBlock *> m_allocated_blocks;

 public:
  ParticleAllocator(ParticlesState &state);
  ParticleAllocator(ParticleAllocator &other) = delete;
  ParticleAllocator(ParticleAllocator &&other) = delete;

  /**
   * Access all blocks that have been allocated by this allocator.
   */
  ArrayRef<AttributesBlock *> allocated_blocks();

  AttributesRefGroup request(StringRef particle_system_name, uint size);

  ParticlesState &particles_state();

 private:
  /**
   * Return a block that can hold new particles. It might create an entirely new one or use a
   * cached block.
   */
  AttributesBlock &get_non_full_block(AttributesBlockContainer &container);

  /**
   * Allocate space for a given number of new particles. The attribute buffers might be distributed
   * over multiple blocks.
   */
  void allocate_block_ranges(StringRef particle_system_name,
                             uint size,
                             Vector<AttributesBlock *> &r_blocks,
                             Vector<IndexRange> &r_ranges);

  const AttributesInfo &attributes_info(StringRef particle_system_name);

  void initialize_new_particles(AttributesBlock &block,
                                AttributesBlockContainer &container,
                                IndexRange pindices);
};

/* ParticleAllocator inline functions
 ********************************************/

inline ParticlesState &ParticleAllocator::particles_state()
{
  return m_state;
}

inline ArrayRef<AttributesBlock *> ParticleAllocator::allocated_blocks()
{
  return m_allocated_blocks;
}

}  // namespace BParticles
