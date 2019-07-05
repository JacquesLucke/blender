#include "actions.hpp"

#include "BLI_hash.h"

namespace BParticles {

Action::~Action()
{
}

class NoneAction : public Action {
  void execute(EventExecuteInterface &UNUSED(interface)) override
  {
  }
};

class ChangeDirectionAction : public Action {
 private:
  SharedFunction m_compute_direction_fn;
  TupleCallBody *m_compute_direction_body;
  Action *m_post_action;

 public:
  ChangeDirectionAction(SharedFunction &compute_direction_fn, Action *post_action)
      : m_compute_direction_fn(compute_direction_fn), m_post_action(post_action)
  {
    m_compute_direction_body = m_compute_direction_fn->body<TupleCallBody>();
  }

  ~ChangeDirectionAction()
  {
    delete m_post_action;
  }

  void execute(EventExecuteInterface &interface) override
  {
    ParticleSet particles = interface.particles();
    auto velocities = particles.attributes().get_float3("Velocity");
    auto position_offsets = interface.attribute_offsets().get_float3("Position");
    auto velocity_offsets = interface.attribute_offsets().get_float3("Velocity");

    FN_TUPLE_CALL_ALLOC_TUPLES(m_compute_direction_body, fn_in, fn_out);

    FN::ExecutionStack stack;
    FN::ExecutionContext execution_context(stack);

    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);
      m_compute_direction_body->call(fn_in, fn_out, execution_context);
      float3 direction = fn_out.get<float3>(0);
      velocities[pindex] = direction;
      position_offsets[pindex] = direction * interface.remaining_time_in_step(i);
      velocity_offsets[pindex] = float3(0);
    }

    m_post_action->execute(interface);
  }
};

class KillAction : public Action {
  void execute(EventExecuteInterface &interface) override
  {
    interface.kill(interface.particles().indices());
  }
};

class MoveAction : public Action {
 private:
  float3 m_offset;

 public:
  MoveAction(float3 offset) : m_offset(offset)
  {
  }

  void execute(EventExecuteInterface &interface) override
  {
    ParticleSet &particles = interface.particles();

    auto positions = particles.attributes().get_float3("Position");
    for (uint pindex : particles.indices()) {
      positions[pindex] += m_offset;
    }
  }
};

class SpawnAction : public Action {
  void execute(EventExecuteInterface &interface) override
  {
    ParticleSet &particles = interface.particles();

    auto positions = particles.attributes().get_float3("Position");

    SmallVector<float3> new_positions;
    SmallVector<float3> new_velocities;
    SmallVector<uint> original_indices;

    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);
      new_positions.append(positions[pindex] + float3(20, 0, 0));
      new_velocities.append(float3(1, 1, 10));
      original_indices.append(i);
    }

    auto &target = interface.request_emit_target(0, original_indices);
    target.set_float3("Position", new_positions);
    target.set_float3("Velocity", new_velocities);
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
  SharedFunction m_compute_amount_fn;
  TupleCallBody *m_compute_amount_body;
  std::unique_ptr<Action> m_post_action;

 public:
  ExplodeAction(StringRef new_particle_name,
                SharedFunction &compute_amount_fn,
                std::unique_ptr<Action> post_action)
      : m_new_particle_name(new_particle_name.to_std_string()),
        m_compute_amount_fn(compute_amount_fn),
        m_post_action(std::move(post_action))
  {
    m_compute_amount_body = m_compute_amount_fn->body<TupleCallBody>();
  }

  void execute(EventExecuteInterface &interface) override
  {
    ParticleSet &particles = interface.particles();

    auto positions = particles.attributes().get_float3("Position");

    SmallVector<float3> new_positions;
    SmallVector<float3> new_velocities;
    SmallVector<uint> original_indices;

    FN_TUPLE_CALL_ALLOC_TUPLES(m_compute_amount_body, fn_in, fn_out);

    FN::ExecutionStack stack;
    FN::ExecutionContext execution_context(stack);
    m_compute_amount_body->call(fn_in, fn_out, execution_context);

    uint parts_amount = std::max(0, fn_out.get<int>(0));
    for (uint i : particles.range()) {
      uint pindex = particles.get_particle_index(i);

      new_positions.append_n_times(positions[pindex], parts_amount);
      original_indices.append_n_times(i, parts_amount);

      for (uint j = 0; j < parts_amount; j++) {
        new_velocities.append(random_direction() * 4);
      }
    }

    auto &target = interface.request_emit_target(m_new_particle_name, original_indices);
    target.set_float3("Position", new_positions);
    target.set_float3("Velocity", new_velocities);

    m_post_action->execute(interface);
  }
};

Action *ACTION_none()
{
  return new NoneAction();
}

Action *ACTION_change_direction(SharedFunction &compute_direction_fn, Action *post_action)
{
  return new ChangeDirectionAction(compute_direction_fn, post_action);
}

Action *ACTION_kill()
{
  return new KillAction();
}

Action *ACTION_move(float3 offset)
{
  return new MoveAction(offset);
}

Action *ACTION_spawn()
{
  return new SpawnAction();
}

Action *ACTION_explode(StringRef new_particle_name,
                       SharedFunction &compute_amount_fn,
                       Action *post_action)
{
  return new ExplodeAction(
      new_particle_name, compute_amount_fn, std::unique_ptr<Action>(post_action));
}

}  // namespace BParticles
