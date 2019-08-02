#include "actions.hpp"

#include "BLI_hash.h"

namespace BParticles {

using BLI::rgba_f;

void NoneAction::execute(ActionInterface &UNUSED(interface))
{
}

void ChangeDirectionAction::execute(ActionInterface &interface)
{
  ParticleSet particles = interface.particles();
  auto velocities = particles.attributes().get<float3>("Velocity");
  auto position_offsets = interface.attribute_offsets().try_get<float3>("Position");
  auto velocity_offsets = interface.attribute_offsets().try_get<float3>("Velocity");

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex : particles.pindices()) {
    float3 direction = inputs->get<float3>("Direction", 0, pindex);

    velocities[pindex] = direction;

    if (position_offsets.has_value()) {
      position_offsets.value()[pindex] = direction * interface.remaining_time_in_step(pindex);
    }
    if (velocity_offsets.has_value()) {
      velocity_offsets.value()[pindex] = float3(0);
    }
  }

  m_post_action->execute(interface);
}

void ChangeColorAction::execute(ActionInterface &interface)
{
  ParticleSet particles = interface.particles();
  auto colors = particles.attributes().get<rgba_f>("Color");

  auto inputs = m_compute_inputs->compute(interface);
  for (uint pindex : particles.pindices()) {
    rgba_f color = inputs->get<rgba_f>("Color", 0, pindex);
    colors[pindex] = color;
  }

  m_post_action->execute(interface);
}

void KillAction::execute(ActionInterface &interface)
{
  interface.kill(interface.particles().pindices());
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

void ExplodeAction::execute(ActionInterface &interface)
{
  ParticleSet &particles = interface.particles();

  auto positions = particles.attributes().get<float3>("Position");

  Vector<float3> new_positions;
  Vector<float3> new_velocities;
  Vector<float> new_birth_times;

  auto inputs = m_compute_inputs->compute(interface);

  for (uint pindex : particles.pindices()) {
    uint parts_amount = std::max(0, inputs->get<int>("Amount", 0, pindex));
    float speed = inputs->get<float>("Speed", 1, pindex);

    new_positions.append_n_times(positions[pindex], parts_amount);
    new_birth_times.append_n_times(interface.current_times()[pindex], parts_amount);

    for (uint j = 0; j < parts_amount; j++) {
      new_velocities.append(random_direction() * speed);
    }
  }

  auto new_particles = interface.particle_allocator().request(m_new_particle_name,
                                                              new_birth_times.size());
  new_particles.set<float3>("Position", new_positions);
  new_particles.set<float3>("Velocity", new_velocities);
  new_particles.fill<float>("Size", 0.1f);
  new_particles.set<float>("Birth Time", new_birth_times);

  m_post_action->execute(interface);
  m_new_particle_action->execute_for_new_particles(new_particles, interface);
}

void ConditionAction::execute(ActionInterface &interface)
{
  ParticleSet particles = interface.particles();

  auto inputs = m_compute_inputs->compute(interface);

  Vector<uint> true_pindices, false_pindices;
  for (uint pindex : particles.pindices()) {
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
