#include "events.hpp"

#include "BLI_kdopbvh.h"

#include "BKE_bvhutils.h"

namespace BParticles {

class AgeReachedEvent : public EventFilter {
 private:
  float m_age;

 public:
  AgeReachedEvent(float age) : m_age(age)
  {
  }

  void filter(EventFilterInterface &interface) override
  {
    ParticleSet particles = interface.particles();
    auto birth_times = particles.attributes().get_float("Birth Time");
    float end_time = interface.end_time();

    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);
      float duration = interface.durations()[i];
      float birth_time = birth_times[pindex];
      float age = end_time - birth_time;
      if (age >= m_age && age - duration < m_age) {
        float time_factor = TimeSpan(end_time - duration, duration).get_factor(birth_time + m_age);
        interface.trigger_particle(i, time_factor);
      }
    }
  }
};

class MeshBounceEvent : public Event {
 private:
  BVHTreeFromMesh *m_treedata;
  float4x4 m_normal_transform;
  float4x4 m_ray_transform;

  struct CollisionData {
    float3 normal;
  };

 public:
  MeshBounceEvent(BVHTreeFromMesh *treedata, float4x4 transform)
      : m_treedata(treedata),
        m_normal_transform(transform),
        m_ray_transform(transform.inverted__LocRotScale())
  {
  }

  uint storage_size() override
  {
    return sizeof(CollisionData);
  }

  void filter(EventFilterInterface &interface) override
  {
    ParticleSet &particles = interface.particles();
    auto positions = particles.attributes().get_float3("Position");
    auto position_offsets = interface.attribute_offsets().get_float3("Position");

    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);

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
        float time_factor = hit.dist / length;
        auto &data = interface.trigger_particle<CollisionData>(i, time_factor);
        data.normal = m_normal_transform.transform_direction(hit.no).normalized();
      }
    }
  }

  void execute(EventExecuteInterface &interface) override
  {
    ParticleSet &particles = interface.particles();

    auto velocities = particles.attributes().get_float3("Velocity");
    auto positions = particles.attributes().get_float3("Position");
    auto position_offsets = interface.attribute_offsets().get_float3("Position");

    for (uint pindex : particles.indices()) {
      auto &data = interface.get_storage<CollisionData>(pindex);

      velocities[pindex].reflect(data.normal);
      position_offsets[pindex].reflect(data.normal);

      /* Temporary solution to avoid double collision. */
      positions[pindex] += velocities[pindex] * 0.0001f;
    }
  }
};

EventFilter *EVENT_age_reached(float age)
{
  return new AgeReachedEvent(age);
}

Event *EVENT_mesh_bounce(BVHTreeFromMesh *treedata, const float4x4 &transform)
{
  return new MeshBounceEvent(treedata, transform);
}

}  // namespace BParticles
