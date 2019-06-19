#include "BParticles.h"
#include "core.hpp"
#include "particles_container.hpp"
#include "playground_solver.hpp"
#include "emitters.hpp"
#include "forces.hpp"

#include "BLI_timeit.hpp"

#define WRAPPERS(T1, T2) \
  inline T1 unwrap(T2 value) \
  { \
    return (T1)value; \
  } \
  inline T2 wrap(T1 value) \
  { \
    return (T2)value; \
  }

using BParticles::AttributeArrays;
using BParticles::AttributeType;
using BParticles::Description;
using BParticles::EmitterHelper;
using BParticles::EmitterInfoBuilder;
using BParticles::EmitterTarget;
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

BParticlesDescription BParticles_playground_description(float control1,
                                                        float control2,
                                                        float *emitter_position,
                                                        struct Mesh *mesh)
{
  auto force = BParticles::FORCE_directional({0.0, 0.0, control1});

  auto emitter1 = BParticles::EMITTER_point(emitter_position);
  auto emitter2 = BParticles::EMITTER_mesh_surface(mesh, control2);

  Description *description = new Description({force.release()},
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
  SCOPED_TIMER("step");
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
