#include "actions.hpp"

#include "BLI_hash.h"

namespace BParticles {

class NoneAction : public Action {
  void execute(ActionInterface &UNUSED(interface)) override
  {
  }
};

class ChangeDirectionAction : public Action {
 private:
  ParticleFunction m_compute_inputs;
  std::unique_ptr<Action> m_post_action;

 public:
  ChangeDirectionAction(ParticleFunction &compute_inputs, std::unique_ptr<Action> post_action)
      : m_compute_inputs(compute_inputs), m_post_action(std::move(post_action))
  {
  }

  void execute(ActionInterface &interface) override
  {
    ParticleSet particles = interface.particles();
    auto velocities = particles.attributes().get_float3("Velocity");
    auto position_offsets = interface.attribute_offsets().get_float3("Position");
    auto velocity_offsets = interface.attribute_offsets().get_float3("Velocity");

    auto caller = m_compute_inputs.get_caller(particles.attributes(), interface.event_info());

    FN_TUPLE_CALL_ALLOC_TUPLES(caller.body(), fn_in, fn_out);

    FN::ExecutionStack stack;
    FN::ExecutionContext execution_context(stack);

    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);

      caller.call(fn_in, fn_out, execution_context, pindex);
      float3 direction = fn_out.get<float3>(0);

      velocities[pindex] = direction;
      position_offsets[pindex] = direction * interface.remaining_time_in_step(pindex);
      velocity_offsets[pindex] = float3(0);
    }

    m_post_action->execute(interface);
  }
};

class KillAction : public Action {
  void execute(ActionInterface &interface) override
  {
    interface.kill(interface.particles().indices());
  }
};

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

class ExplodeAction : public Action {
 private:
  std::string m_new_particle_name;
  ParticleFunction m_compute_inputs;
  std::unique_ptr<Action> m_post_action;

 public:
  ExplodeAction(StringRef new_particle_name,
                ParticleFunction &compute_inputs,
                std::unique_ptr<Action> post_action)
      : m_new_particle_name(new_particle_name.to_std_string()),
        m_compute_inputs(compute_inputs),
        m_post_action(std::move(post_action))
  {
  }

  void execute(ActionInterface &interface) override
  {
    ParticleSet &particles = interface.particles();

    auto positions = particles.attributes().get_float3("Position");

    SmallVector<float3> new_positions;
    SmallVector<float3> new_velocities;
    SmallVector<float> new_birth_times;

    auto caller = m_compute_inputs.get_caller(particles.attributes(), interface.event_info());
    FN_TUPLE_CALL_ALLOC_TUPLES(caller.body(), fn_in, fn_out);

    FN::ExecutionStack stack;
    FN::ExecutionContext execution_context(stack);

    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);

      caller.call(fn_in, fn_out, execution_context, pindex);
      uint parts_amount = std::max(0, fn_out.get<int>(0));
      float speed = fn_out.get<float>(1);

      new_positions.append_n_times(positions[pindex], parts_amount);
      new_birth_times.append_n_times(interface.current_times()[i], parts_amount);

      for (uint j = 0; j < parts_amount; j++) {
        new_velocities.append(random_direction() * speed);
      }
    }

    auto target = interface.particle_allocator().request(m_new_particle_name,
                                                         new_birth_times.size());
    target.set_float3("Position", new_positions);
    target.set_float3("Velocity", new_velocities);
    target.fill_float("Size", 0.1f);
    target.set_float("Birth Time", new_birth_times);

    m_post_action->execute(interface);
  }
};

class ConditionAction : public Action {
 private:
  ParticleFunction m_compute_inputs;
  std::unique_ptr<Action> m_true_action, m_false_action;

 public:
  ConditionAction(ParticleFunction &compute_inputs,
                  std::unique_ptr<Action> true_action,
                  std::unique_ptr<Action> false_action)
      : m_compute_inputs(compute_inputs),
        m_true_action(std::move(true_action)),
        m_false_action(std::move(false_action))
  {
  }

  void execute(ActionInterface &interface) override
  {
    ParticleSet particles = interface.particles();
    SmallVector<bool> conditions(particles.size());
    this->compute_conditions(interface, conditions);

    SmallVector<uint> true_indices, false_indices;
    for (uint i : particles.range()) {
      if (conditions[i]) {
        true_indices.append(i);
      }
      else {
        false_indices.append(i);
      }
    }

    interface.execute_action_for_subset(true_indices, m_true_action);
    interface.execute_action_for_subset(false_indices, m_false_action);
  }

  void compute_conditions(ActionInterface &interface, ArrayRef<bool> r_conditions)
  {
    ParticleSet particles = interface.particles();
    BLI_assert(particles.size() == r_conditions.size());

    auto caller = m_compute_inputs.get_caller(particles.attributes(), interface.event_info());
    FN_TUPLE_CALL_ALLOC_TUPLES(caller.body(), fn_in, fn_out);

    FN::ExecutionStack stack;
    FN::ExecutionContext execution_context(stack);
    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);
      caller.call(fn_in, fn_out, execution_context, pindex);
      bool condition = fn_out.get<bool>(0);
      r_conditions[i] = condition;
    }
  }
};

std::unique_ptr<Action> ACTION_none()
{
  Action *action = new NoneAction();
  return std::unique_ptr<Action>(action);
}

std::unique_ptr<Action> ACTION_change_direction(ParticleFunction &compute_inputs,
                                                std::unique_ptr<Action> post_action)
{
  Action *action = new ChangeDirectionAction(compute_inputs, std::move(post_action));
  return std::unique_ptr<Action>(action);
}

std::unique_ptr<Action> ACTION_kill()
{
  Action *action = new KillAction();
  return std::unique_ptr<Action>(action);
}

std::unique_ptr<Action> ACTION_explode(StringRef new_particle_name,
                                       ParticleFunction &compute_inputs,
                                       std::unique_ptr<Action> post_action)
{
  Action *action = new ExplodeAction(new_particle_name, compute_inputs, std::move(post_action));
  return std::unique_ptr<Action>(action);
}

std::unique_ptr<Action> ACTION_condition(ParticleFunction &compute_inputs,
                                         std::unique_ptr<Action> true_action,
                                         std::unique_ptr<Action> false_action)
{
  Action *action = new ConditionAction(
      compute_inputs, std::move(true_action), std::move(false_action));
  return std::unique_ptr<Action>(action);
}

}  // namespace BParticles
