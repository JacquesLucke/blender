#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

Force::~Force()
{
}

class DirectionalForce : public Force {
 private:
  float3 m_force;

 public:
  DirectionalForce(float3 force) : m_force(force)
  {
  }

  void add_force(ParticlesBlock &block, ArrayRef<float3> r_force) override
  {
    for (uint i = 0; i < block.active_amount(); i++) {
      r_force[i] += m_force;
    }
  };
};

class TurbulenceForce : public BParticles::Force {
 private:
  float m_strength;

 public:
  TurbulenceForce(float strength) : m_strength(strength)
  {
  }

  void add_force(ParticlesBlock &block, ArrayRef<float3> r_force) override
  {
    auto positions = block.attributes().get_float3("Position");

    for (uint pindex = 0; pindex < block.active_amount(); pindex++) {
      float3 pos = positions[pindex];
      float value = BLI_hnoise(0.5f, pos.x, pos.y, pos.z);
      r_force[pindex].z += value * m_strength;
    }
  }
};

Force *FORCE_directional(float3 force)
{
  return new DirectionalForce(force);
}

Force *FORCE_turbulence(float strength)
{
  return new TurbulenceForce(strength);
}

}  // namespace BParticles
