#include "BKE_falloff.hpp"

#include "BLI_array_cxx.h"

namespace BKE {

using BLI::TemporaryArray;

Falloff::~Falloff()
{
}

void ConstantFalloff::compute(AttributesRef UNUSED(attributes),
                              ArrayRef<uint> indices,
                              MutableArrayRef<float> r_weights) const
{
  for (uint index : indices) {
    r_weights[index] = m_weight;
  }
}

void PointDistanceFalloff::compute(AttributesRef attributes,
                                   ArrayRef<uint> indices,
                                   MutableArrayRef<float> r_weights) const
{
  auto positions = attributes.get<float3>("Position");
  float distance_diff = m_max_distance - m_min_distance;

  for (uint index : indices) {
    float3 position = positions[index];
    float distance = float3::distance(position, m_point);

    float weight = 0;
    if (distance_diff > 0) {
      weight = 1.0f - (distance - m_min_distance) / distance_diff;
      CLAMP(weight, 0.0f, 1.0f);
    }
    r_weights[index] = weight;
  }
}

MeshDistanceFalloff::MeshDistanceFalloff(Object *object,
                                         float inner_distance,
                                         float outer_distance)
    : m_object(object), m_inner_distance(inner_distance), m_outer_distance(outer_distance)
{
  BLI_assert(object->type == OB_MESH);
  m_local_to_world = m_object->obmat;
  m_world_to_local = m_local_to_world.inverted__LocRotScale();

  BKE_bvhtree_from_mesh_get(&m_bvhtree_data, (Mesh *)object->data, BVHTREE_FROM_LOOPTRI, 2);
}

MeshDistanceFalloff::~MeshDistanceFalloff()
{
  free_bvhtree_from_mesh(&m_bvhtree_data);
}

void MeshDistanceFalloff::compute(AttributesRef attributes,
                                  ArrayRef<uint> indices,
                                  MutableArrayRef<float> r_weights) const
{
  auto positions = attributes.get<float3>("Position");

  float distance_diff = std::max(0.0001f, m_outer_distance - m_inner_distance);
  for (uint index : indices) {
    float3 position = positions[index];
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
      r_weights[index] = 0.0f;
      continue;
    }

    float3 nearest_position = m_local_to_world.transform_position(nearest.co);
    float distance = float3::distance(position, nearest_position);

    float weight = 1.0f - (distance - m_inner_distance) / distance_diff;
    CLAMP(weight, 0.0f, 1.0f);
    r_weights[index] = weight;
  }
}

}  // namespace BKE
