#include "events.hpp"

namespace BParticles {

void AgeReachedEvent::attributes(AttributesDeclaration &builder)
{
  builder.add_byte(m_identifier, 0);
}

void AgeReachedEvent::filter(EventFilterInterface &interface)
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

void AgeReachedEvent::execute(EventExecuteInterface &interface)
{
  ParticleSet particles = interface.particles();

  auto was_activated_before = particles.attributes().get_byte(m_identifier);
  for (uint pindex : particles.pindices()) {
    was_activated_before[pindex] = true;
  }

  ActionInterface::RunFromEvent(m_action, interface);
}

void MeshCollisionEvent::attributes(AttributesDeclaration &builder)
{
  builder.add_float(m_identifier, 0.0f);
}

uint MeshCollisionEvent::storage_size()
{
  return sizeof(EventStorage);
}

void MeshCollisionEvent::filter(EventFilterInterface &interface)
{
  ParticleSet particles = interface.particles();
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

MeshCollisionEvent::RayCastResult MeshCollisionEvent::ray_cast(float3 start,
                                                               float3 normalized_direction,
                                                               float max_distance)
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

void MeshCollisionEvent::execute(EventExecuteInterface &interface)
{
  ParticleSet particles = interface.particles();
  Vector<float3> normals(particles.block().active_amount());
  auto last_collision_times = particles.attributes().get_float(m_identifier);

  for (uint pindex : particles.pindices()) {
    auto storage = interface.get_storage<EventStorage>(pindex);
    normals[pindex] = storage.normal;
    last_collision_times[pindex] = interface.current_times()[pindex];
  }

  CollisionEventInfo event_info(normals);
  ActionInterface::RunFromEvent(m_action, interface, &event_info);
}

void CloseByPointsEvent::filter(EventFilterInterface &interface)
{
  ParticleSet particles = interface.particles();
  auto positions = particles.attributes().get_float3("Position");

  for (uint pindex : particles.pindices()) {
    KDTreeNearest_3d nearest;
    if (BLI_kdtree_3d_find_nearest(m_kdtree, positions[pindex], &nearest) != -1) {
      if (float3::distance(positions[pindex], nearest.co) < m_distance) {
        interface.trigger_particle(pindex, 0.5f);
      }
    }
  }
}

void CloseByPointsEvent::execute(EventExecuteInterface &interface)
{
  ActionInterface::RunFromEvent(m_action, interface);
}

}  // namespace BParticles
