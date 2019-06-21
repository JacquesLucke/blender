#include "events.hpp"

#include "BLI_kdopbvh.h"

#include "BKE_bvhutils.h"

namespace BParticles {

class AgeReachedEvent : public Event {
 private:
  float m_age;

 public:
  AgeReachedEvent(float age) : m_age(age)
  {
  }

  void filter(ParticleSet particles,
              IdealOffsets &UNUSED(ideal_offsets),
              ArrayRef<float> durations,
              float end_time,
              SmallVector<uint> &r_filtered_indices,
              SmallVector<float> &r_time_factors) override
  {
    auto birth_times = particles.attributes().get_float("Birth Time");

    for (uint i : particles.range()) {
      uint pindex = particles.pindex_of(i);
      float duration = durations[i];
      float birth_time = birth_times[pindex];
      float age = end_time - birth_time;
      if (age >= m_age && age - duration < m_age) {
        r_filtered_indices.append(i);
        float time_factor = TimeSpan(end_time - duration, duration).get_factor(birth_time + m_age);
        r_time_factors.append(time_factor);
      }
    }
  }
};

class MeshCollisionEvent : public Event {
 private:
  BVHTreeFromMesh *m_treedata;
  float4x4 m_ray_transform;

 public:
  MeshCollisionEvent(BVHTreeFromMesh *treedata, float4x4 transform)
      : m_treedata(treedata), m_ray_transform(transform.inverted__LocRotScale())
  {
  }

  void filter(ParticleSet particles,
              IdealOffsets &ideal_offsets,
              ArrayRef<float> UNUSED(durations),
              float UNUSED(end_time),
              SmallVector<uint> &r_filtered_indices,
              SmallVector<float> &r_time_factors) override
  {
    auto positions = particles.attributes().get_float3("Position");
    auto position_offsets = ideal_offsets.position_offsets;

    for (uint i : particles.range()) {
      uint pindex = particles.pindex_of(i);

      float3 start_position = m_ray_transform.transform_position(positions[pindex]);
      float3 direction = m_ray_transform.transform_direction(position_offsets[i]);
      float length = direction.normalize_and_get_length();

      BVHTreeRayHit hit;
      hit.dist = length;
      hit.index = -1;
      BLI_bvhtree_ray_cast(m_treedata->tree,
                           start_position,
                           direction,
                           0.0f,
                           &hit,
                           m_treedata->raycast_callback,
                           (void *)m_treedata);

      if (hit.index != -1) {
        float time_factor = hit.dist / direction.length();
        r_filtered_indices.append(i);
        r_time_factors.append(time_factor);
      }
    }
  }
};

std::unique_ptr<Event> EVENT_age_reached(float age)
{
  Event *event = new AgeReachedEvent(age);
  return std::unique_ptr<Event>(event);
}

std::unique_ptr<Event> EVENT_mesh_collection(BVHTreeFromMesh *treedata, const float4x4 &transform)
{
  Event *event = new MeshCollisionEvent(treedata, transform);
  return std::unique_ptr<Event>(event);
}

}  // namespace BParticles
