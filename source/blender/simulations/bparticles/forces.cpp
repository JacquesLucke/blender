#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

Force::~Force()
{
}

void GravityForce::add_force(ForceInterface &interface)
{
  ParticlesBlock &block = interface.block();
  ArrayRef<float3> destination = interface.combined_destination();

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex = 0; pindex < block.active_amount(); pindex++) {
    float3 acceleration = inputs->get<float3>("Direction", 0, pindex);
    destination[pindex] += acceleration;
  }
};

void TurbulenceForce::add_force(ForceInterface &interface)
{
  ParticlesBlock &block = interface.block();
  ArrayRef<float3> destination = interface.combined_destination();

  auto positions = block.attributes().get<float3>("Position");

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex = 0; pindex < block.active_amount(); pindex++) {
    float3 pos = positions[pindex];
    float3 strength = inputs->get<float3>("Strength", 0, pindex);
    float x = (BLI_gNoise(0.5f, pos.x, pos.y, pos.z + 1000.0f, false, 1) - 0.5f) * strength.x;
    float y = (BLI_gNoise(0.5f, pos.x, pos.y + 1000.0f, pos.z, false, 1) - 0.5f) * strength.y;
    float z = (BLI_gNoise(0.5f, pos.x + 1000.0f, pos.y, pos.z, false, 1) - 0.5f) * strength.z;
    destination[pindex] += {x, y, z};
  }
}

void PointForce::add_force(ForceInterface &interface)
{
  ParticlesBlock &block = interface.block();
  ArrayRef<float3> destination = interface.combined_destination();

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex = 0; pindex < block.active_amount(); pindex++) {
    float3 direction = inputs->get<float3>("Direction", 0, pindex);
    float strength = inputs->get<float>("Strength", 1, pindex);
    float falloff = inputs->get<float>("Falloff", 2, pindex);
    float distance = inputs->get<float>("Distance", 3, pindex);
    bool gravitation = inputs->get<bool>("Gravitation", 4, pindex);

    if (gravitation) {
      if (distance < FLT_EPSILON) {
        strength = 0.0f;
      }
      else {
        strength *= powf(distance, -2.0f);
      }
    }

    direction *= (strength * falloff);

    destination[pindex] += direction;
  }
}

}  // namespace BParticles
