#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

Force::~Force()
{
}

void GravityForce::add_force(ForceInterface &interface)
{
  MutableArrayRef<float3> destination = interface.combined_destination();

  auto inputs = m_inputs_fn->compute(interface);

  for (uint pindex : interface.pindices()) {
    float3 acceleration = inputs->get<float3>("Acceleration", 0, pindex);
    float weight = inputs->get<float>("Weight", 1, pindex);
    destination[pindex] += acceleration * weight;
  }
};

void TurbulenceForce::add_force(ForceInterface &interface)
{
  MutableArrayRef<float3> destination = interface.combined_destination();
  auto positions = interface.attributes().get<float3>("Position");

  auto inputs = m_inputs_fn->compute(interface);

  for (uint pindex : interface.pindices()) {
    float3 pos = positions[pindex];
    float3 strength = inputs->get<float3>("Strength", 0, pindex);
    float size = inputs->get<float>("Size", 1, pindex);
    float weight = inputs->get<float>("Weight", 2, pindex);
    float x = (BLI_gNoise(size, pos.x, pos.y, pos.z + 1000.0f, false, 1) - 0.5f) * strength.x;
    float y = (BLI_gNoise(size, pos.x, pos.y + 1000.0f, pos.z, false, 1) - 0.5f) * strength.y;
    float z = (BLI_gNoise(size, pos.x + 1000.0f, pos.y, pos.z, false, 1) - 0.5f) * strength.z;
    destination[pindex] += float3(x, y, z) * weight;
  }
}

void DragForce::add_force(ForceInterface &interface)
{
  MutableArrayRef<float3> destination = interface.combined_destination();
  auto velocities = interface.attributes().get<float3>("Velocity");

  auto inputs = m_inputs_fn->compute(interface);

  for (uint pindex : interface.pindices()) {
    float3 velocity = velocities[pindex];
    float strength = inputs->get<float>("Strength", 0, pindex);
    float weight = inputs->get<float>("Weight", 1, pindex);
    destination[pindex] -= velocity * strength * weight;
  }
}

void MeshForce::add_force(ForceInterface &interface)
{
  MutableArrayRef<float3> destination = interface.combined_destination();
  auto positions = interface.attributes().get<float3>("Position");

  auto inputs = m_inputs_fn->compute(interface);

  for (uint pindex : interface.pindices()) {
    float3 position = positions[pindex];
    float3 local_position = m_world_to_local.transform_position(position);

    BVHTreeNearest nearest = {0};
    nearest.dist_sq = 10000.0f;
    nearest.index = -1;
    BLI_bvhtree_find_nearest(m_bvhtree_data.tree,
                             local_position,
                             &nearest,
                             m_bvhtree_data.nearest_callback,
                             (void *)&m_bvhtree_data);

    if (nearest.index == -1) {
      continue;
    }

    float3 difference_local = float3(nearest.co) - local_position;
    float3 difference = m_local_to_world.transform_direction(difference_local);
    float distance_squared = difference.length_squared();
    float factor = 1 / std::max(0.1f, distance_squared);

    float strength = inputs->get<float>("Strength", 1, pindex);
    destination[pindex] += difference * strength * factor;
  }
}

}  // namespace BParticles
