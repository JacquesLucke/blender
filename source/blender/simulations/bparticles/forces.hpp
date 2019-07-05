#pragma once

#include "core.hpp"
#include "actions.hpp"

namespace BParticles {

class Force {
 public:
  virtual ~Force() = 0;
  virtual void add_force(ParticlesBlock &block, ArrayRef<float3> r_force) = 0;
};

Force *FORCE_gravity(SharedFunction &compute_acceleration_fn);
Force *FORCE_turbulence(float strength);

}  // namespace BParticles
