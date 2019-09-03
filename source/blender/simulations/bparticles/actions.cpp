#include "actions.hpp"
#include "action_contexts.hpp"

#include "BLI_hash.h"

namespace BParticles {

using BLI::rgba_f;

void NoneAction::execute(ActionInterface &UNUSED(interface))
{
}

void ActionSequence::execute(ActionInterface &interface)
{
  for (auto &action : m_actions) {
    action->execute(interface);
  }
}

static float random_number()
{
  static uint number = 0;
  number++;
  return BLI_hash_int_01(number) * 2.0f - 1.0f;
}

static float3 random_direction()
{
  return float3(random_number(), random_number(), random_number());
}

static void update_position_and_velocity_offsets(ActionInterface &interface)
{
  AttributesRef attributes = interface.attributes();
  AttributesRef attribute_offsets = interface.attribute_offsets();

  auto velocities = attributes.get<float3>("Velocity");
  auto position_offsets = attribute_offsets.try_get<float3>("Position");
  auto velocity_offsets = attribute_offsets.try_get<float3>("Velocity");

  for (uint pindex : interface.pindices()) {
    float3 velocity = velocities[pindex];

    if (position_offsets.has_value()) {
      position_offsets.value()[pindex] = velocity * interface.remaining_time_in_step(pindex);
    }
    if (velocity_offsets.has_value()) {
      velocity_offsets.value()[pindex] = float3(0);
    }
  }
}

void SetVelocityAction::execute(ActionInterface &interface)
{
  auto velocities = interface.attributes().get<float3>("Velocity");

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex : interface.pindices()) {
    float3 velocity = inputs->get<float3>("Velocity", 0, pindex);
    velocities[pindex] = velocity;
  }

  update_position_and_velocity_offsets(interface);
}

void RandomizeVelocityAction::execute(ActionInterface &interface)
{
  auto velocities = interface.attributes().get<float3>("Velocity");

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex : interface.pindices()) {
    float randomness = inputs->get<float>("Randomness", 0, pindex);
    float3 old_velocity = velocities[pindex];
    float old_speed = old_velocity.length();

    float3 velocity_offset = random_direction().normalized() * old_speed * randomness;
    velocities[pindex] += velocity_offset;
  }

  update_position_and_velocity_offsets(interface);
}

void ChangeColorAction::execute(ActionInterface &interface)
{
  auto colors = interface.attributes().get<rgba_f>("Color");

  auto inputs = m_compute_inputs->compute(interface);
  for (uint pindex : interface.pindices()) {
    rgba_f color = inputs->get<rgba_f>("Color", 0, pindex);
    colors[pindex] = color;
  }
}

void ChangeSizeAction::execute(ActionInterface &interface)
{
  auto sizes = interface.attributes().get<float>("Size");

  auto inputs = m_compute_inputs->compute(interface);
  for (uint pindex : interface.pindices()) {
    float size = inputs->get<float>("Size", 0, pindex);
    sizes[pindex] = size;
  }
}

void KillAction::execute(ActionInterface &interface)
{
  interface.kill(interface.pindices());
}

void ExplodeAction::execute(ActionInterface &interface)
{
  auto positions = interface.attributes().get<float3>("Position");

  Vector<float3> new_positions;
  Vector<float3> new_velocities;
  Vector<float> new_birth_times;
  Vector<uint> source_particles;

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex : interface.pindices()) {
    uint parts_amount = std::max(0, inputs->get<int>("Amount", 0, pindex));
    float speed = inputs->get<float>("Speed", 1, pindex);

    source_particles.append_n_times(pindex, parts_amount);
    new_positions.append_n_times(positions[pindex], parts_amount);
    new_birth_times.append_n_times(interface.current_times()[pindex], parts_amount);

    for (uint j = 0; j < parts_amount; j++) {
      new_velocities.append(random_direction() * speed);
    }
  }

  for (StringRef type_name : m_types_to_emit) {
    auto new_particles = interface.particle_allocator().request(type_name, new_birth_times.size());
    new_particles.set<float3>("Position", new_positions);
    new_particles.set<float3>("Velocity", new_velocities);
    new_particles.fill<float>("Size", 0.1f);
    new_particles.set<float>("Birth Time", new_birth_times);

    SourceParticleActionContext source_context(source_particles, &interface.context());

    m_on_birth_action->execute_for_new_particles(new_particles, interface, &source_context);
  }
}

void ConditionAction::execute(ActionInterface &interface)
{
  auto inputs = m_compute_inputs->compute(interface);

  Vector<uint> true_pindices, false_pindices;
  for (uint pindex : interface.pindices()) {
    if (inputs->get<bool>("Condition", 0, pindex)) {
      true_pindices.append(pindex);
    }
    else {
      false_pindices.append(pindex);
    }
  }

  m_true_action->execute_for_subset(true_pindices, interface);
  m_false_action->execute_for_subset(false_pindices, interface);
}

}  // namespace BParticles
