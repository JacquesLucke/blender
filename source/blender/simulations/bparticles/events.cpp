#include "events.hpp"

#include "action_contexts.hpp"

namespace BParticles {

/* Age Reached Event
 ******************************************/

void AgeReachedEvent::attributes(AttributesDeclaration &builder)
{
  builder.add<uint8_t>(m_identifier, 0);
}

void AgeReachedEvent::filter(EventFilterInterface &interface)
{
  AttributeArrays attributes = interface.attributes();
  auto birth_times = attributes.get<float>("Birth Time");
  auto was_activated_before = attributes.get<uint8_t>(m_identifier);

  float end_time = interface.step_end_time();

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex : interface.pindices()) {
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
  auto was_activated_before = interface.attributes().get<uint8_t>(m_identifier);
  for (uint pindex : interface.pindices()) {
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
  AttributeArrays attributes = interface.attributes();
  auto positions = attributes.get<float3>("Position");
  auto last_collision_times = attributes.get<float>(m_identifier);
  auto position_offsets = interface.attribute_offsets().get<float3>("Position");

  for (uint pindex : interface.pindices()) {
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
      storage.local_normal = result.normal;
      storage.local_position = ray_start + ray_direction * result.distance;
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
  uint array_size = interface.array_size();
  TemporaryArray<float3> local_positions(array_size);
  TemporaryArray<float3> local_normals(array_size);
  TemporaryArray<uint> looptri_indices(array_size);
  TemporaryArray<float4x4> world_transforms(array_size);
  TemporaryArray<float3> world_normals(array_size);

  auto last_collision_times = interface.attributes().get<float>(m_identifier);

  for (uint pindex : interface.pindices()) {
    auto storage = interface.get_storage<EventStorage>(pindex);
    looptri_indices[pindex] = storage.looptri_index;
    local_positions[pindex] = storage.local_position;
    local_normals[pindex] = storage.local_normal;
    world_transforms[pindex] = m_local_to_world;
    world_normals[pindex] =
        m_local_to_world.transform_direction(storage.local_normal).normalized();

    last_collision_times[pindex] = interface.current_times()[pindex];
  }

  MeshCollisionContext action_context(
      m_object, world_transforms, local_positions, local_normals, world_normals, looptri_indices);

  m_action->execute_from_event(interface, &action_context);
}

}  // namespace BParticles
