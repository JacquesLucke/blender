#pragma once

#include "core.hpp"

namespace BParticles {

class Force {
 public:
  virtual ~Force() = 0;
  virtual void add_force(ParticlesBlock &block, ArrayRef<float3> r_force) = 0;
};

Force *FORCE_directional(float3 force);
Force *FORCE_turbulence(float strength);

}  // namespace BParticles
