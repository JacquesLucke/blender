#include "actions.hpp"

#include "BLI_hash.h"

namespace BParticles {

void NoneAction::execute(ActionInterface &UNUSED(interface))
{
}

void ChangeDirectionAction::execute(ActionInterface &interface)
{
  ParticleSet particles = interface.particles();
  auto velocities = particles.attributes().get_float3("Velocity");
  auto position_offsets = interface.attribute_offsets().try_get_float3("Position");
  auto velocity_offsets = interface.attribute_offsets().try_get_float3("Velocity");

  auto caller = m_compute_inputs.get_caller(particles.attributes(), interface.event_info());

  FN_TUPLE_CALL_ALLOC_TUPLES(caller.body(), fn_in, fn_out);

  FN::ExecutionStack stack;
  FN::ExecutionContext execution_context(stack);

  for (uint pindex : particles.pindices()) {
    caller.call(fn_in, fn_out, execution_context, pindex);
    float3 direction = fn_out.get<float3>(0);

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

  auto positions = particles.attributes().get_float3("Position");

  Vector<float3> new_positions;
  Vector<float3> new_velocities;
  Vector<float> new_birth_times;

  auto caller = m_compute_inputs.get_caller(particles.attributes(), interface.event_info());
  FN_TUPLE_CALL_ALLOC_TUPLES(caller.body(), fn_in, fn_out);

  FN::ExecutionStack stack;
  FN::ExecutionContext execution_context(stack);

  for (uint pindex : particles.pindices()) {
    caller.call(fn_in, fn_out, execution_context, pindex);
    uint parts_amount = std::max(0, fn_out.get<int>(0));
    float speed = fn_out.get<float>(1);

    new_positions.append_n_times(positions[pindex], parts_amount);
    new_birth_times.append_n_times(interface.current_times()[pindex], parts_amount);

    for (uint j = 0; j < parts_amount; j++) {
      new_velocities.append(random_direction() * speed);
    }
  }

  auto new_particles = interface.particle_allocator().request(m_new_particle_name,
                                                              new_birth_times.size());
  new_particles.set_float3("Position", new_positions);
  new_particles.set_float3("Velocity", new_velocities);
  new_particles.fill_float("Size", 0.1f);
  new_particles.set_float("Birth Time", new_birth_times);

  m_post_action->execute(interface);
}

void ConditionAction::execute(ActionInterface &interface)
{
  ParticleSet particles = interface.particles();
  ArrayAllocator::Array<bool> conditions(interface.array_allocator());
  this->compute_conditions(interface, conditions);

  Vector<uint> true_pindices, false_pindices;
  for (uint pindex : particles.pindices()) {
    if (conditions[pindex]) {
      true_pindices.append(pindex);
    }
    else {
      false_pindices.append(pindex);
    }
  }

  m_true_action->execute_for_subset(true_pindices, interface);
  m_false_action->execute_for_subset(false_pindices, interface);
}

void ConditionAction::compute_conditions(ActionInterface &interface, ArrayRef<bool> r_conditions)
{
  ParticleSet particles = interface.particles();

  auto caller = m_compute_inputs.get_caller(particles.attributes(), interface.event_info());
  FN_TUPLE_CALL_ALLOC_TUPLES(caller.body(), fn_in, fn_out);

  FN::ExecutionStack stack;
  FN::ExecutionContext execution_context(stack);
  for (uint pindex : particles.pindices()) {
    caller.call(fn_in, fn_out, execution_context, pindex);
    bool condition = fn_out.get<bool>(0);
    r_conditions[pindex] = condition;
  }
}

}  // namespace BParticles
