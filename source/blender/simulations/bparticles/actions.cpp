#include "actions.hpp"

#include "BLI_hash.h"

namespace BParticles {

Action::~Action()
{
}

class NoneAction : public Action {
  void execute(EventExecuteInterface &UNUSED(interface), EventInfo &UNUSED(event_info)) override
  {
  }
};

class ChangeDirectionAction : public Action {
 private:
  ParticleFunction m_compute_inputs;
  Action *m_post_action;

 public:
  ChangeDirectionAction(ParticleFunction &compute_inputs, Action *post_action)
      : m_compute_inputs(compute_inputs), m_post_action(post_action)
  {
  }

  ~ChangeDirectionAction()
  {
    delete m_post_action;
  }

  void execute(EventExecuteInterface &interface, EventInfo &event_info) override
  {
    ParticleSet particles = interface.particles();
    auto velocities = particles.attributes().get_float3("Velocity");
    auto position_offsets = interface.attribute_offsets().get_float3("Position");
    auto velocity_offsets = interface.attribute_offsets().get_float3("Velocity");

    auto caller = m_compute_inputs.get_caller(particles.attributes(), event_info);

    FN_TUPLE_CALL_ALLOC_TUPLES(caller.body(), fn_in, fn_out);

    FN::ExecutionStack stack;
    FN::ExecutionContext execution_context(stack);

    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);

      caller.call(fn_in, fn_out, execution_context, pindex);
      float3 direction = fn_out.get<float3>(0);

      velocities[pindex] = direction;
      position_offsets[pindex] = direction * interface.remaining_time_in_step(i);
      velocity_offsets[pindex] = float3(0);
    }

    m_post_action->execute(interface, event_info);
  }
};

class KillAction : public Action {
  void execute(EventExecuteInterface &interface, EventInfo &UNUSED(event_info)) override
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

  void execute(EventExecuteInterface &interface, EventInfo &event_info) override
  {
    ParticleSet &particles = interface.particles();

    auto positions = particles.attributes().get_float3("Position");

    SmallVector<float3> new_positions;
    SmallVector<float3> new_velocities;
    SmallVector<uint> original_indices;

    auto caller = m_compute_inputs.get_caller(particles.attributes(), event_info);
    FN_TUPLE_CALL_ALLOC_TUPLES(caller.body(), fn_in, fn_out);

    FN::ExecutionStack stack;
    FN::ExecutionContext execution_context(stack);

    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);

      caller.call(fn_in, fn_out, execution_context, pindex);
      uint parts_amount = std::max(0, fn_out.get<int>(0));
      float speed = fn_out.get<float>(1);

      new_positions.append_n_times(positions[pindex], parts_amount);
      original_indices.append_n_times(i, parts_amount);

      for (uint j = 0; j < parts_amount; j++) {
        new_velocities.append(random_direction() * speed);
      }
    }

    auto &target = interface.request_emit_target(m_new_particle_name, original_indices);
    target.set_float3("Position", new_positions);
    target.set_float3("Velocity", new_velocities);

    m_post_action->execute(interface, event_info);
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

  void execute(EventExecuteInterface &interface, EventInfo &event_info) override
  {
    SmallVector<uint> true_indices, false_indices;
    SmallVector<float> true_times, false_times;

    ParticleSet particles = interface.particles();
    auto current_times = interface.current_times();

    auto caller = m_compute_inputs.get_caller(particles.attributes(), event_info);
    FN_TUPLE_CALL_ALLOC_TUPLES(caller.body(), fn_in, fn_out);

    FN::ExecutionStack stack;
    FN::ExecutionContext execution_context(stack);
    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);
      caller.call(fn_in, fn_out, execution_context, pindex);
      bool condition = fn_out.get<bool>(0);
      if (condition) {
        true_indices.append(pindex);
        true_times.append(current_times[i]);
      }
      else {
        false_indices.append(pindex);
        false_times.append(current_times[i]);
      }
    }

    ParticleSet true_particles(particles.block(), true_indices);
    EventExecuteInterface true_interface(true_particles,
                                         interface.block_allocator(),
                                         true_times,
                                         interface.event_storage(),
                                         interface.attribute_offsets(),
                                         interface.step_end_time());
    m_true_action->execute(true_interface, event_info);

    ParticleSet false_particles(particles.block(), false_indices);
    EventExecuteInterface false_interface(false_particles,
                                          interface.block_allocator(),
                                          false_times,
                                          interface.event_storage(),
                                          interface.attribute_offsets(),
                                          interface.step_end_time());
    m_false_action->execute(false_interface, event_info);
  }
};

Action *ACTION_none()
{
  return new NoneAction();
}

Action *ACTION_change_direction(ParticleFunction &compute_inputs, Action *post_action)
{
  return new ChangeDirectionAction(compute_inputs, post_action);
}

Action *ACTION_kill()
{
  return new KillAction();
}

Action *ACTION_explode(StringRef new_particle_name,
                       ParticleFunction &compute_inputs,
                       Action *post_action)
{
  return new ExplodeAction(
      new_particle_name, compute_inputs, std::unique_ptr<Action>(post_action));
}

Action *ACTION_condition(ParticleFunction &compute_inputs,
                         Action *true_action,
                         Action *false_action)
{
  return new ConditionAction(
      compute_inputs, std::unique_ptr<Action>(true_action), std::unique_ptr<Action>(false_action));
}

}  // namespace BParticles
