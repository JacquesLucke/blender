#include "BParticles.h"
#include "core.hpp"
#include "particles_container.hpp"
#include "playground_solver.hpp"

#define WRAPPERS(T1, T2) \
  inline T1 unwrap(T2 value) \
  { \
    return (T1)value; \
  } \
  inline T2 wrap(T1 value) \
  { \
    return (T2)value; \
  }

using BParticles::Description;
using BParticles::NamedBuffers;
using BParticles::ParticlesBlock;
using BParticles::Solver;
using BParticles::StateBase;
using BParticles::WrappedState;

using BLI::ArrayRef;
using BLI::Vec3;

WRAPPERS(BParticles::Description *, BParticlesDescription);
WRAPPERS(BParticles::Solver *, BParticlesSolver);
WRAPPERS(BParticles::WrappedState *, BParticlesState);

class TestForce : public BParticles::Force {
 private:
  float m_value_1, m_value_2;

 public:
  TestForce(float value_1, float value_2) : m_value_1(value_1), m_value_2(value_2)
  {
  }

  void add_force(NamedBuffers &UNUSED(buffers), ArrayRef<Vec3> dst) override
  {
    for (uint i = 0; i < dst.size(); i++) {
      dst[i].x += m_value_1;
      dst[i].z += m_value_2;
    }
  };
};

class TestEmitter : public BParticles::Emitter {
 public:
  void emit(std::function<ParticlesBlock *()> request_block) override
  {
    ParticlesBlock *block = request_block();

    uint index = block->next_inactive_index();
    block->vec3_buffer("Position")[index] = {(float)(rand() % 100) / 30.0f, 0, 1};
    block->vec3_buffer("Velocity")[index] = {0, 0.1, 0};
    block->float_buffer("Age")[index] = 0;
    block->active_amount()++;
  }
};

BParticlesDescription BParticles_playground_description(float control1, float control2)
{
  Description *description = new Description({new TestForce(control1, control2)},
                                             {new TestEmitter()});
  return wrap(description);
}
void BParticles_description_free(BParticlesDescription description_c)
{
  delete unwrap(description_c);
}

BParticlesSolver BParticles_solver_build(BParticlesDescription description_c)
{
  Description *description = unwrap(description_c);
  return wrap(BParticles::new_playground_solver(*description));
}
void BParticles_solver_free(BParticlesSolver solver_c)
{
  delete unwrap(solver_c);
}

BParticlesState BParticles_state_init(BParticlesSolver solver_c)
{
  Solver *solver = unwrap(solver_c);
  StateBase *state = solver->init();
  return wrap(new WrappedState(solver, std::unique_ptr<StateBase>(state)));
}
void BParticles_state_adapt(BParticlesSolver new_solver_c, BParticlesState state_to_adapt_c)
{
  Solver *new_solver = unwrap(new_solver_c);
  WrappedState *wrapped_state = unwrap(state_to_adapt_c);
  BParticles::adapt_state(new_solver, wrapped_state);
}
void BParticles_state_step(BParticlesSolver solver_c, BParticlesState state_c)
{
  Solver *solver = unwrap(solver_c);
  WrappedState *wrapped_state = unwrap(state_c);

  BLI_assert(solver == &wrapped_state->solver());
  solver->step(*wrapped_state);
}
void BParticles_state_free(BParticlesState state_c)
{
  delete unwrap(state_c);
}

uint BParticles_state_particle_count(BParticlesSolver solver_c, BParticlesState state_c)
{
  Solver *solver = unwrap(solver_c);
  WrappedState *wrapped_state = unwrap(state_c);
  return solver->particle_amount(*wrapped_state);
}
void BParticles_state_get_positions(BParticlesSolver solver_c,
                                    BParticlesState state_c,
                                    float (*dst)[3])
{
  Solver *solver = unwrap(solver_c);
  WrappedState *wrapped_state = unwrap(state_c);
  solver->get_positions(*wrapped_state, dst);
}
