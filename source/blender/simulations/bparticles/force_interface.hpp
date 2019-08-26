#pragma once

#include "step_description_interfaces.hpp"

namespace BParticles {

class ForceInterface : public BlockStepDataAccess {
 private:
  ArrayRef<uint> m_pindices;
  MutableArrayRef<float3> m_destination;

 public:
  ForceInterface(BlockStepData &step_data,
                 ArrayRef<uint> pindices,
                 MutableArrayRef<float3> destination)
      : BlockStepDataAccess(step_data), m_pindices(pindices), m_destination(destination)
  {
  }

  ParticleSet particles()
  {
    return ParticleSet(this->attributes(), m_pindices);
  }

  MutableArrayRef<float3> combined_destination()
  {
    return m_destination;
  }
};

}  // namespace BParticles
