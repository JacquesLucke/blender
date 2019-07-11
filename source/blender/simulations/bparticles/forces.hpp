#pragma once

#include "core.hpp"
#include "actions.hpp"

namespace BParticles {

class Force {
 public:
  virtual ~Force() = 0;
  virtual void add_force(ParticlesBlock &block, ArrayRef<float3> r_force) = 0;
};

std::unique_ptr<Force> FORCE_gravity(SharedFunction &compute_acceleration_fn);
std::unique_ptr<Force> FORCE_turbulence(SharedFunction &compute_strength_fn);

}  // namespace BParticles
