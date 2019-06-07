#include "BLI_math.h"
#include "BLI_small_vector.hpp"

#include "BParticles.h"

using BLI::SmallVector;

#define WRAPPERS(T1, T2) \
  inline T1 unwrap(T2 value) \
  { \
    return (T1)value; \
  } \
  inline T2 wrap(T1 value) \
  { \
    return (T2)value; \
  }

namespace BParticles {

class Description;
class Solver;
class State;

class Description {
 public:
  virtual ~Description();
};

class Solver {
 public:
  virtual ~Solver();

  virtual void step(State *state) const = 0;
};

class State {
 public:
  virtual ~State();

  virtual Solver *solver() const = 0;
};

Description::~Description()
{
}
Solver::~Solver()
{
}
State::~State()
{
}

}  // namespace BParticles

struct Vector {
  float x, y, z;
};

using BParticles::Description;
using BParticles::Solver;
using BParticles::State;

WRAPPERS(BParticles::Description *, BParticlesDescription);
WRAPPERS(BParticles::Solver *, BParticlesSolver);
WRAPPERS(BParticles::State *, BParticlesState);

BParticlesDescription BParticles_playground_description()
{
  return wrap(new Description());
}
void BParticles_description_free(BParticlesDescription description_c)
{
  delete unwrap(description_c);
}

class SimpleState : public State {
 private:
  SmallVector<Vector> m_positions;

 public:
  Solver *m_solver;

  SimpleState(Solver *solver) : m_solver(solver)
  {
  }

  Solver *solver() const override
  {
    return m_solver;
  }

  SmallVector<Vector> &positions()
  {
    return m_positions;
  }
};

class SimpleSolver : public Solver {
 private:
  Description *m_description;

 public:
  SimpleSolver(Description *description) : m_description(description)
  {
  }

  void step(State *state_) const override
  {
    SimpleState *state = (SimpleState *)state_;
    for (Vector &position : state->positions()) {
      position.x += 0.1f;
    }
    state->positions().append({0, 0, 1});
  }
};

BParticlesSolver BParticles_solver_build(BParticlesDescription description_c)
{
  return wrap(new SimpleSolver(unwrap(description_c)));
}
void BParticles_solver_free(BParticlesSolver solver_c)
{
  delete unwrap(solver_c);
}

BParticlesState BParticles_state_init(BParticlesSolver solver_c)
{
  return wrap(new SimpleState(unwrap(solver_c)));
}
void BParticles_state_adapt(BParticlesSolver new_solver_c, BParticlesState *state_to_adapt_c)
{
  SimpleState *state = (SimpleState *)unwrap(*state_to_adapt_c);
  state->m_solver = unwrap(new_solver_c);
}
void BParticles_state_step(BParticlesState state_c)
{
  State *state = unwrap(state_c);
  Solver *solver = state->solver();
  solver->step(state);
}
void BParticles_state_free(BParticlesState state_c)
{
  delete unwrap(state_c);
}

uint BParticles_state_particle_count(BParticlesState state_c)
{
  SimpleState *state = (SimpleState *)unwrap(state_c);
  return state->positions().size();
}
void BParticles_state_get_positions(BParticlesState state_c, float (*dst)[3])
{
  SimpleState *state = (SimpleState *)unwrap(state_c);
  memcpy(dst, state->positions().begin(), state->positions().size() * sizeof(Vector));
}
