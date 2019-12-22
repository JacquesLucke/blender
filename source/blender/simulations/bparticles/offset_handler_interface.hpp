#pragma once

#include "BLI_index_mask.h"

#include "block_step_data.hpp"
#include "particle_allocator.hpp"

namespace BParticles {

using BLI::IndexMask;

class OffsetHandlerInterface : public BlockStepDataAccess {
 private:
  IndexMask m_mask;
  ArrayRef<float> m_time_factors;
  ParticleAllocator &m_particle_allocator;

 public:
  OffsetHandlerInterface(BlockStepData &step_data,
                         IndexMask mask,
                         ArrayRef<float> time_factors,
                         ParticleAllocator &particle_allocator)
      : BlockStepDataAccess(step_data),
        m_mask(mask),
        m_time_factors(time_factors),
        m_particle_allocator(particle_allocator)
  {
  }

  ArrayRef<uint> mask()
  {
    return m_mask;
  }

  ArrayRef<float> time_factors()
  {
    return m_time_factors;
  }

  ParticleAllocator &particle_allocator()
  {
    return m_particle_allocator;
  }
};

class OffsetHandler {
 public:
  virtual ~OffsetHandler()
  {
  }

  virtual void execute(OffsetHandlerInterface &interface) = 0;
};

}  // namespace BParticles
