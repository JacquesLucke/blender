#include "events.hpp"

#include "BLI_kdopbvh.h"

#include "BKE_bvhutils.h"

#include "DNA_object_types.h"

namespace BParticles {

class AgeReachedEvent : public Event {
 private:
  std::string m_identifier;
  SharedFunction m_compute_age_fn;
  TupleCallBody *m_compute_age_body;
  std::unique_ptr<Action> m_action;

 public:
  AgeReachedEvent(StringRef identifier,
                  SharedFunction &compute_age_fn,
                  std::unique_ptr<Action> action)
      : m_identifier(identifier.to_std_string()),
        m_compute_age_fn(compute_age_fn),
        m_action(std::move(action))
  {
    m_compute_age_body = compute_age_fn->body<TupleCallBody>();
  }

  void attributes(AttributesInfoBuilder &builder) override
  {
    builder.use_byte(m_identifier, 0);
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

    for (uint pindex : particles.pindices()) {
      if (was_activated_before[pindex]) {
        continue;
      }

      float birth_time = birth_times[pindex];
      float age_at_end = end_time - birth_time;

      if (age_at_end >= trigger_age) {
        TimeSpan time_span = interface.time_span(pindex);

        float age_at_start = age_at_end - time_span.duration();
        if (trigger_age < age_at_start) {
          interface.trigger_particle(pindex, 0.0f);
        }
        else {
          float time_factor = time_span.get_factor_safe(birth_time + trigger_age);
          CLAMP(time_factor, 0.0f, 1.0f);
          interface.trigger_particle(pindex, time_factor);
        }
      }
    }
  }

  void execute(EventExecuteInterface &interface) override
  {
    ParticleSet particles = interface.particles();

    auto was_activated_before = particles.attributes().get_byte(m_identifier);
    for (uint pindex : particles.pindices()) {
      was_activated_before[pindex] = true;
    }

    ActionInterface::RunFromEvent(m_action, interface);
  }
};

class CollisionEventInfo : public EventInfo {
 private:
  ArrayRef<float3> m_normals;

 public:
  CollisionEventInfo(ArrayRef<float3> normals) : m_normals(normals)
  {
  }

  void *get_info_array(StringRef UNUSED(name)) override
  {
    return (void *)m_normals.begin();
  }
};

class MeshCollisionEventFilter : public Event {
 private:
  std::string m_identifier;
  Object *m_object;
  BVHTreeFromMesh m_bvhtree_data;
  float4x4 m_local_to_world;
  float4x4 m_world_to_local;
  std::unique_ptr<Action> m_action;

  struct RayCastResult {
    bool success;
    int index;
    float3 normal;
    float distance;
  };

  struct EventStorage {
    float3 normal;
  };

 public:
  MeshCollisionEventFilter(StringRef identifier, Object *object, std::unique_ptr<Action> action)
      : m_identifier(identifier.to_std_string()), m_object(object), m_action(std::move(action))
  {
    BLI_assert(object->type == OB_MESH);
    m_local_to_world = m_object->obmat;
    m_world_to_local = m_local_to_world.inverted__LocRotScale();

    BKE_bvhtree_from_mesh_get(&m_bvhtree_data, (Mesh *)object->data, BVHTREE_FROM_LOOPTRI, 2);
  }

  ~MeshCollisionEventFilter()
  {
    free_bvhtree_from_mesh(&m_bvhtree_data);
  }

  void attributes(AttributesInfoBuilder &builder) override
  {
    builder.use_float(m_identifier, 0.0f);
  }

  uint storage_size() override
  {
    return sizeof(EventStorage);
  }

  void filter(EventFilterInterface &interface) override
  {
    ParticleSet &particles = interface.particles();
    auto positions = particles.attributes().get_float3("Position");
    auto last_collision_times = particles.attributes().get_float(m_identifier);
    auto position_offsets = interface.attribute_offsets().get_float3("Position");

    for (uint pindex : particles.pindices()) {
      float3 ray_start = m_world_to_local.transform_position(positions[pindex]);
      float3 ray_direction = m_world_to_local.transform_direction(position_offsets[pindex]);
      float length = ray_direction.normalize_and_get_length();

      auto result = this->ray_cast(ray_start, ray_direction, length);
      if (result.success) {
        float time_factor = result.distance / length;
        float time = interface.time_span(pindex).interpolate(time_factor);
        if (std::abs(last_collision_times[pindex] - time) < 0.0001f) {
          continue;
        }
        auto &storage = interface.trigger_particle<EventStorage>(pindex, time_factor);
        if (float3::dot(result.normal, ray_direction) > 0) {
          result.normal = -result.normal;
        }
        storage.normal = m_local_to_world.transform_direction(result.normal).normalized();
      }
    }
  }

  RayCastResult ray_cast(float3 start, float3 normalized_direction, float max_distance)
  {
    BVHTreeRayHit hit;
    hit.dist = max_distance;
    hit.index = -1;
    BLI_bvhtree_ray_cast(m_bvhtree_data.tree,
                         start,
                         normalized_direction,
                         0.0f,
                         &hit,
                         m_bvhtree_data.raycast_callback,
                         (void *)&m_bvhtree_data);

    return {hit.index >= 0, hit.index, float3(hit.no), hit.dist};
  }

  void execute(EventExecuteInterface &interface) override
  {
    ParticleSet &particles = interface.particles();
    SmallVector<float3> normals(particles.block().active_amount());
    auto last_collision_times = particles.attributes().get_float(m_identifier);

    for (uint pindex : particles.pindices()) {
      auto storage = interface.get_storage<EventStorage>(pindex);
      normals[pindex] = storage.normal;
      last_collision_times[pindex] = interface.current_times()[pindex];
    }

    CollisionEventInfo event_info(normals);
    ActionInterface::RunFromEvent(m_action, interface, &event_info);
  }
};

std::unique_ptr<Event> EVENT_age_reached(StringRef identifier,
                                         SharedFunction &compute_age_fn,
                                         std::unique_ptr<Action> action)
{
  Event *event = new AgeReachedEvent(identifier, compute_age_fn, std::move(action));
  return std::unique_ptr<Event>(event);
}

std::unique_ptr<Event> EVENT_mesh_collision(StringRef identifier,
                                            Object *object,
                                            std::unique_ptr<Action> action)
{
  Event *event = new MeshCollisionEventFilter(identifier, object, std::move(action));
  return std::unique_ptr<Event>(event);
}

}  // namespace BParticles
