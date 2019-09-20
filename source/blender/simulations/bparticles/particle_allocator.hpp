#pragma once

#include "particles_state.hpp"

namespace BParticles {

using BKE::AttributesRefGroup;

class ParticleAllocator : BLI::NonCopyable, BLI::NonMovable {
 private:
  ParticlesState &m_state;
  Map<AttributesBlockContainer *, AttributesBlock *> m_non_full_cache;
  Vector<AttributesBlock *> m_allocated_blocks;
  std::mutex m_request_mutex;

 public:
  ParticleAllocator(ParticlesState &state);

  /**
   * Access all blocks that have been allocated by this allocator.
   */
  ArrayRef<AttributesBlock *> allocated_blocks();

  /**
   * Get memory buffers for new particles.
   */
  AttributesRefGroup request(StringRef particle_system_name, uint size);

 private:
  /**
   * Return a block that can hold new particles. It might create an entirely new one or use a
   * cached block.
   */
  AttributesBlock &get_non_full_block(AttributesBlockContainer &container);

  /**
   * Allocate space for a given number of new particles. The attribute buffers might be
   * distributed over multiple blocks.
   */
  void allocate_buffer_ranges(AttributesBlockContainer &container,
                              uint size,
                              Vector<ArrayRef<void *>> &r_buffers,
                              Vector<IndexRange> &r_ranges);

  void initialize_new_particles(AttributesBlockContainer &container,
                                AttributesRefGroup &attributes_group);
};

/* ParticleAllocator inline functions
 ********************************************/

inline ArrayRef<AttributesBlock *> ParticleAllocator::allocated_blocks()
{
  return m_allocated_blocks;
}

}  // namespace BParticles
