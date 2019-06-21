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

  void filter(AttributeArrays attributes,
              ArrayRef<uint> particle_indices,
              IdealOffsets &UNUSED(ideal_offsets),
              ArrayRef<float> durations,
              float end_time,
              SmallVector<uint> &r_filtered_indices,
              SmallVector<float> &r_time_factors) override
  {
    auto birth_times = attributes.get_float("Birth Time");

    for (uint i = 0; i < particle_indices.size(); i++) {
      uint pindex = particle_indices[i];
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

 public:
  MeshCollisionEvent(BVHTreeFromMesh *treedata) : m_treedata(treedata)
  {
  }

  void filter(AttributeArrays attributes,
              ArrayRef<uint> particle_indices,
              IdealOffsets &ideal_offsets,
              ArrayRef<float> UNUSED(durations),
              float UNUSED(end_time),
              SmallVector<uint> &r_filtered_indices,
              SmallVector<float> &r_time_factors) override
  {
    auto positions = attributes.get_float3("Position");
    auto position_offsets = ideal_offsets.position_offsets;

    for (uint i = 0; i < particle_indices.size(); i++) {
      uint pindex = particle_indices[i];

      float3 start_position = positions[pindex];
      float3 direction = position_offsets[i];
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

std::unique_ptr<Event> EVENT_mesh_collection(BVHTreeFromMesh *treedata)
{
  Event *event = new MeshCollisionEvent(treedata);
  return std::unique_ptr<Event>(event);
}

}  // namespace BParticles
