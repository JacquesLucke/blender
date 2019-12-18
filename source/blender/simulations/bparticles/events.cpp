#include "BLI_utildefines.h"
#include "BLI_hash.h"

#include "events.hpp"

namespace BParticles {

using BLI::LargeScopedVector;

/* Age Reached Event
 ******************************************/

void AgeReachedEvent::filter(EventFilterInterface &interface)
{
  AttributesRef attributes = interface.attributes();
  auto ids = attributes.get<int32_t>("ID");

  auto inputs = ParticleFunctionResult::Compute(
      *m_inputs_fn, interface.pindices(), interface.attributes());

  LargeScopedArray<float> trigger_ages(attributes.size());
  for (uint pindex : interface.pindices()) {
    float age = inputs.get_single<float>("Age", 0, pindex);
    float variation = inputs.get_single<float>("Variation", 1, pindex);
    int32_t id = ids[pindex];
    float random_factor = BLI_hash_int_01(id);
    trigger_ages[pindex] = age + random_factor * variation;
  }

  float end_time = interface.step_end_time();
  auto birth_times = attributes.get<float>("Birth Time");
  auto was_activated_before = attributes.get<bool>(m_is_triggered_attribute);

  for (uint pindex : interface.pindices()) {
    if (was_activated_before[pindex]) {
      continue;
    }

    float trigger_age = trigger_ages[pindex];
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
  auto was_activated_before = interface.attributes().get<bool>(m_is_triggered_attribute);
  for (uint pindex : interface.pindices()) {
    was_activated_before[pindex] = true;
  }

  m_action.execute_from_event(interface);
}

/* Custom Event
 ***********************************************/

void CustomEvent::filter(EventFilterInterface &interface)
{
  auto was_activated_before = interface.attributes().get<bool>(m_is_triggered_attribute);

  LargeScopedVector<uint> pindices_to_check;
  pindices_to_check.reserve(interface.pindices().size());

  for (uint pindex : interface.pindices()) {
    if (!was_activated_before[pindex]) {
      pindices_to_check.append(pindex);
    }
  }

  auto inputs = ParticleFunctionResult::Compute(
      *m_inputs_fn, pindices_to_check, interface.attributes());

  for (uint pindex : pindices_to_check) {
    bool condition = inputs.get_single<bool>("Condition", 0, pindex);
    if (condition) {
      interface.trigger_particle(pindex, 0.0f);
    }
  }
}

void CustomEvent::execute(EventExecuteInterface &interface)
{
  auto was_activated_before = interface.attributes().get<bool>(m_is_triggered_attribute);
  for (uint pindex : interface.pindices()) {
    was_activated_before[pindex] = true;
  }

  m_action.execute_from_event(interface);
}

/* Collision Event
 ***********************************************/

uint MeshCollisionEvent::storage_size()
{
  return sizeof(EventStorage);
}

void MeshCollisionEvent::filter(EventFilterInterface &interface)
{
  AttributesRef attributes = interface.attributes();
  auto positions = attributes.get<float3>("Position");
  auto last_collision_step = attributes.get<int32_t>(m_last_collision_attribute);
  auto position_offsets = interface.attribute_offsets().get<float3>("Position");

  uint current_update_index = interface.simulation_state().time().current_update_index();

  for (uint pindex : interface.pindices()) {
    if (last_collision_step[pindex] == current_update_index) {
      continue;
    }

    float3 world_ray_start = positions[pindex];
    float3 world_ray_direction = position_offsets[pindex];
    float3 world_ray_end = world_ray_start + world_ray_direction;

    float3 local_ray_start = m_world_to_local_begin.transform_position(world_ray_start);
    float3 local_ray_end = m_world_to_local_end.transform_position(world_ray_end);
    float3 local_ray_direction = local_ray_end - local_ray_start;
    float local_ray_length = local_ray_direction.normalize_and_get_length();

    auto result = this->ray_cast(local_ray_start, local_ray_direction, local_ray_length);
    if (result.success) {
      float time_factor = result.distance / local_ray_length;
      auto &storage = interface.trigger_particle<EventStorage>(pindex, time_factor);
      if (float3::dot(result.normal, local_ray_direction) > 0) {
        result.normal = -result.normal;
      }
      storage.local_normal = result.normal;
      storage.local_position = local_ray_start + local_ray_direction * result.distance;
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
  auto last_collision_step = interface.attributes().get<int32_t>(m_last_collision_attribute);
  uint current_update_index = interface.simulation_state().time().current_update_index();

  for (uint pindex : interface.pindices()) {
    last_collision_step[pindex] = current_update_index;
  }
  m_action.execute_from_event(interface);
}

}  // namespace BParticles
