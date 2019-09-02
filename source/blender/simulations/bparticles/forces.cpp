#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

Force::~Force()
{
}

void GravityForce::add_force(ForceInterface &interface)
{
  MutableArrayRef<float3> destination = interface.combined_destination();

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex : interface.pindices()) {
    float3 acceleration = inputs->get<float3>("Direction", 0, pindex);
    destination[pindex] += acceleration;
  }
};

void TurbulenceForce::add_force(ForceInterface &interface)
{
  MutableArrayRef<float3> destination = interface.combined_destination();
  auto positions = interface.attributes().get<float3>("Position");

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex : interface.pindices()) {
    float3 pos = positions[pindex];
    float3 strength = inputs->get<float3>("Strength", 0, pindex);
    float x = (BLI_gNoise(0.5f, pos.x, pos.y, pos.z + 1000.0f, false, 1) - 0.5f) * strength.x;
    float y = (BLI_gNoise(0.5f, pos.x, pos.y + 1000.0f, pos.z, false, 1) - 0.5f) * strength.y;
    float z = (BLI_gNoise(0.5f, pos.x + 1000.0f, pos.y, pos.z, false, 1) - 0.5f) * strength.z;
    destination[pindex] += {x, y, z};
  }
}

}  // namespace BParticles
