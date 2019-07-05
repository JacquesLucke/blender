#include "events.hpp"

#include "BLI_kdopbvh.h"

#include "BKE_bvhutils.h"

namespace BParticles {

EventFilter::~EventFilter()
{
}

class AgeReachedEvent : public EventFilter {
 private:
  std::string m_identifier;
  SharedFunction m_compute_age_fn;
  TupleCallBody *m_compute_age_body;

 public:
  AgeReachedEvent(StringRef identifier, SharedFunction &compute_age_fn)
      : m_identifier(identifier.to_std_string()), m_compute_age_fn(compute_age_fn)
  {
    m_compute_age_body = compute_age_fn->body<TupleCallBody>();
  }

  void attributes(TypeAttributeInterface &interface) override
  {
    interface.use(AttributeType::Byte, m_identifier);
  }

  void filter(EventFilterInterface &interface) override
  {
    ParticleSet particles = interface.particles();
    auto birth_times = particles.attributes().get_float("Birth Time");
    auto was_activated_before = particles.attributes().get_byte(m_identifier);

    float end_time = interface.end_time();

    FN_TUPLE_CALL_ALLOC_TUPLES(m_compute_age_body, fn_in, fn_out);
    FN::ExecutionStack stack;
    FN::ExecutionContext execution_context(stack);
    m_compute_age_body->call(fn_in, fn_out, execution_context);
    float trigger_age = fn_out.get<float>(0);

    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);
      if (was_activated_before[pindex]) {
        continue;
      }

      float birth_time = birth_times[pindex];
      float age_at_end = end_time - birth_time;

      if (age_at_end >= trigger_age) {
        TimeSpan time_span = interface.time_span(i);
        float time_factor = time_span.get_factor(birth_time + trigger_age);
        interface.trigger_particle(i, time_factor);
      }
    }
  }

  void triggered(EventExecuteInterface &interface) override
  {
    ParticleSet particles = interface.particles();

    auto was_activated_before = particles.attributes().get_byte(m_identifier);
    for (uint pindex : particles.indices()) {
      was_activated_before[pindex] = true;
    }
  }
};

class MeshBounceEvent : public Event {
 private:
  BVHTreeFromMesh *m_treedata;
  float4x4 m_local_to_world;
  float4x4 m_world_to_local;

  struct EventData {
    float3 hit_normal;
  };

  struct RayCastResult {
    bool success;
    int index;
    float3 normal;
    float distance;
  };

 public:
  MeshBounceEvent(BVHTreeFromMesh *treedata, float4x4 transform)
      : m_treedata(treedata),
        m_local_to_world(transform),
        m_world_to_local(transform.inverted__LocRotScale())
  {
  }

  uint storage_size() override
  {
    return sizeof(EventData);
  }

  void filter(EventFilterInterface &interface) override
  {
    ParticleSet &particles = interface.particles();
    auto positions = particles.attributes().get_float3("Position");
    auto position_offsets = interface.attribute_offsets().get_float3("Position");

    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);

      float3 ray_start = m_world_to_local.transform_position(positions[pindex]);
      float3 ray_direction = m_world_to_local.transform_direction(position_offsets[i]);
      float length = ray_direction.normalize_and_get_length();

      auto result = this->ray_cast(ray_start, ray_direction, length);
      if (result.success) {
        float time_factor = result.distance / length;
        auto &data = interface.trigger_particle<EventData>(i, time_factor);

        float3 normal = result.normal;
        if (float3::dot(normal, ray_direction) > 0) {
          normal.invert();
        }
        data.hit_normal = m_local_to_world.transform_direction(normal).normalized();
      }
    }
  }

  RayCastResult ray_cast(float3 start, float3 normalized_direction, float max_distance)
  {
    BVHTreeRayHit hit;
    hit.dist = max_distance;
    hit.index = -1;
    BLI_bvhtree_ray_cast(m_treedata->tree,
                         start,
                         normalized_direction,
                         0.0f,
                         &hit,
                         m_treedata->raycast_callback,
                         (void *)m_treedata);

    return {hit.index >= 0, hit.index, float3(hit.no), hit.dist};
  }

  void execute(EventExecuteInterface &interface) override
  {
    ParticleSet &particles = interface.particles();

    auto velocities = particles.attributes().get_float3("Velocity");
    auto positions = particles.attributes().get_float3("Position");
    auto position_offsets = interface.attribute_offsets().get_float3("Position");

    for (uint pindex : particles.indices()) {
      auto &data = interface.get_storage<EventData>(pindex);

      /* Move particle back a little bit to avoid double collision. */
      positions[pindex] += data.hit_normal * 0.001f;

      velocities[pindex] = this->bounce_direction(velocities[pindex], data.hit_normal);
      position_offsets[pindex] = this->bounce_direction(position_offsets[pindex], data.hit_normal);
    }
  }

  float3 bounce_direction(float3 direction, float3 normal)
  {
    direction = direction.reflected(normal);

    float normal_part = float3::dot(direction, normal);
    float3 direction_normal = normal * normal_part;
    float3 direction_tangent = direction - direction_normal;

    return direction_normal * 0.5 + direction_tangent * 0.9;
  }
};

EventFilter *EVENT_age_reached(StringRef identifier, SharedFunction &compute_age_fn)
{
  return new AgeReachedEvent(identifier, compute_age_fn);
}

Event *EVENT_mesh_bounce(BVHTreeFromMesh *treedata, const float4x4 &transform)
{
  return new MeshBounceEvent(treedata, transform);
}

}  // namespace BParticles
