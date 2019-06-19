#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

class DirectionalForce : public Force {
 private:
  float3 m_force;

 public:
  DirectionalForce(float3 force) : m_force(force)
  {
  }

  void add_force(AttributeArrays UNUSED(attributes),
                 ArrayRef<uint> indices_mask,
                 ArrayRef<float3> dst) override
  {
    for (uint i = 0; i < indices_mask.size(); i++) {
      dst[i] += m_force;
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

  void add_force(AttributeArrays attributes,
                 ArrayRef<uint> indices_mask,
                 ArrayRef<float3> dst) override
  {
    auto positions = attributes.get_float3("Position");
    for (uint i = 0; i < indices_mask.size(); i++) {
      uint pindex = indices_mask[i];

      float3 pos = positions[pindex];
      float value = BLI_hnoise(0.5f, pos.x, pos.y, pos.z);
      dst[i].z += value * m_strength;
    }
  }
};

std::unique_ptr<Force> FORCE_directional(float3 force)
{
  return std::unique_ptr<Force>(new DirectionalForce(force));
}

std::unique_ptr<Force> FORCE_turbulence(float strength)
{
  return std::unique_ptr<Force>(new TurbulenceForce(strength));
}

}  // namespace BParticles
