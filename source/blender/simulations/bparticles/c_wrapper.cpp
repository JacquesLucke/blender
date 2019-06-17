#include "BParticles.h"
#include "core.hpp"
#include "particles_container.hpp"
#include "playground_solver.hpp"
#include "emitter.hpp"
#include "BLI_noise.h"

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
using BParticles::EmitterBuffers;
using BParticles::EmitterInfoBuilder;
using BParticles::NamedBuffers;
using BParticles::ParticlesBlock;
using BParticles::Solver;
using BParticles::StateBase;
using BParticles::WrappedState;

using BLI::ArrayRef;
using BLI::float3;
using BLI::SmallVector;
using BLI::StringRef;

WRAPPERS(BParticles::Description *, BParticlesDescription);
WRAPPERS(BParticles::Solver *, BParticlesSolver);
WRAPPERS(BParticles::WrappedState *, BParticlesState);

class TestForce : public BParticles::Force {
 private:
  float m_strength;

 public:
  TestForce(float strength) : m_strength(strength)
  {
  }

  void add_force(NamedBuffers &UNUSED(buffers), ArrayRef<float3> dst) override
  {
    for (uint i = 0; i < dst.size(); i++) {
      dst[i].z += m_strength;
    }
  };
};

class TurbulenceForce : public BParticles::Force {
 private:
  float m_strength;

 public:
  TurbulenceForce(float strength) : m_strength(strength)
  {
  }

  void add_force(NamedBuffers &buffers, ArrayRef<float3> dst) override
  {
    auto positions = buffers.get_float3("Position");
    for (uint i = 0; i < dst.size(); i++) {
      float3 pos = positions[i];
      float value = BLI_hnoise(0.5f, pos.x, pos.y, pos.z);
      dst[i].z += value * m_strength;
    }
  }
};

class TestEmitter : public BParticles::Emitter {
 public:
  void info(EmitterInfoBuilder &builder) const override
  {
    builder.inits_float3_attribute("Position");
    builder.inits_float3_attribute("Velocity");
  }

  void emit(BParticles::RequestEmitterBufferCB request_buffers) override
  {
    EmitterBuffers &dst = request_buffers();
    BLI_assert(dst.size() > 0);

    auto positions = dst.buffers().get_float3("Position");
    auto velocities = dst.buffers().get_float3("Velocity");

    for (uint i = 0; i < dst.size(); i++) {
      positions[i] = {(float)(rand() % 10000) / 3000.0f, 0, 1};
      velocities[i] = {0, 1, 1};
    }
    dst.set_initialized(dst.size());
  }
};

BParticlesDescription BParticles_playground_description(float control1,
                                                        float control2,
                                                        float *emitter_position,
                                                        struct Mesh *mesh)
{
  auto emitter1 = BParticles::new_point_emitter(emitter_position);
  auto emitter2 = BParticles::new_surface_emitter(mesh);

  Description *description = new Description(
      {new TestForce(control1), new TurbulenceForce(control2)},
      {emitter1.release(), emitter2.release()});
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
void BParticles_state_step(BParticlesSolver solver_c,
                           BParticlesState state_c,
                           float elapsed_seconds)
{
  Solver *solver = unwrap(solver_c);
  WrappedState *wrapped_state = unwrap(state_c);

  BLI_assert(solver == &wrapped_state->solver());
  solver->step(*wrapped_state, elapsed_seconds);
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
