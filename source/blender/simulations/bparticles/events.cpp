#include "events.hpp"

namespace BParticles {

/* Age Reached Event
 ******************************************/

void AgeReachedEvent::attributes(AttributesDeclaration &builder)
{
  builder.add<uint8_t>(m_identifier, 0);
}

void AgeReachedEvent::filter(EventFilterInterface &interface)
{
  ParticleSet particles = interface.particles();
  auto birth_times = particles.attributes().get<float>("Birth Time");
  auto was_activated_before = particles.attributes().get<uint8_t>(m_identifier);

  float end_time = interface.step_end_time();

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex : particles.pindices()) {
    if (was_activated_before[pindex]) {
      continue;
    }

    float trigger_age = inputs->get<float>("Age", 0, pindex);

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

  auto was_activated_before = particles.attributes().get<uint8_t>(m_identifier);
  for (uint pindex : particles.pindices()) {
    was_activated_before[pindex] = true;
  }

  m_action->execute_from_event(interface);
}

/* Collision Event
 ***********************************************/

void MeshCollisionEvent::attributes(AttributesDeclaration &builder)
{
  builder.add<float>(m_identifier, 0.0f);
}

uint MeshCollisionEvent::storage_size()
{
  return sizeof(EventStorage);
}

void MeshCollisionEvent::filter(EventFilterInterface &interface)
{
  ParticleSet particles = interface.particles();
  auto positions = particles.attributes().get<float3>("Position");
  auto last_collision_times = particles.attributes().get<float>(m_identifier);
  auto position_offsets = interface.attribute_offsets().get<float3>("Position");

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
      storage.looptri_index = result.index;
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
  TemporaryArray<float3> normals(interface.array_size());
  TemporaryArray<uint> looptri_indices(interface.array_size());
  auto last_collision_times = particles.attributes().get<float>(m_identifier);

  for (uint pindex : particles.pindices()) {
    auto storage = interface.get_storage<EventStorage>(pindex);
    looptri_indices[pindex] = storage.looptri_index;
    normals[pindex] = storage.normal;
    last_collision_times[pindex] = interface.current_times()[pindex];
  }

  CollisionEventInfo action_context(m_object, looptri_indices, normals);
  m_action->execute_from_event(interface, &action_context);
}

void CloseByPointsEvent::filter(EventFilterInterface &interface)
{
  ParticleSet particles = interface.particles();
  auto positions = particles.attributes().get<float3>("Position");

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
  m_action->execute_from_event(interface);
}

}  // namespace BParticles
