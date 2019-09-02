#pragma once

#include "block_step_data.hpp"
#include "particle_allocator.hpp"

namespace BParticles {

class OffsetHandlerInterface : public BlockStepDataAccess {
 private:
  ArrayRef<uint> m_pindices;
  ArrayRef<float> m_time_factors;
  ParticleAllocator &m_particle_allocator;

 public:
  OffsetHandlerInterface(BlockStepData &step_data,
                         ArrayRef<uint> pindices,
                         ArrayRef<float> time_factors,
                         ParticleAllocator &particle_allocator)
      : BlockStepDataAccess(step_data),
        m_pindices(pindices),
        m_time_factors(time_factors),
        m_particle_allocator(particle_allocator)
  {
  }

  ArrayRef<uint> pindices()
  {
    return m_pindices;
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
