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
class WrappedState;
class StateBase;

class Description {
 public:
  virtual ~Description();
};

class Solver {
 public:
  virtual ~Solver();

  virtual WrappedState *init() = 0;
  virtual void step(WrappedState &wrapped_state) = 0;

  virtual uint particle_amount(WrappedState &wrapped_state) = 0;
  virtual void get_positions(WrappedState &wrapped_state, float (*dst)[3]) = 0;
};

class StateBase {
 public:
  virtual ~StateBase();
};

class WrappedState final {
 private:
  Solver *m_solver;
  std::unique_ptr<StateBase> m_state;

 public:
  WrappedState(Solver *solver, std::unique_ptr<StateBase> state)
      : m_solver(solver), m_state(std::move(state))
  {
    BLI_assert(solver);
    BLI_assert(m_state.get() != NULL);
  }

  WrappedState(WrappedState &other) = delete;
  WrappedState(WrappedState &&other) = delete;
  WrappedState &operator=(WrappedState &other) = delete;
  WrappedState &operator=(WrappedState &&other) = delete;

  Solver &solver() const
  {
    BLI_assert(m_solver);
    return *m_solver;
  }

  template<typename T> T &state() const
  {
    T *state = dynamic_cast<T *>(m_state.get());
    BLI_assert(state);
    return *state;
  }

  friend void ::BParticles_state_adapt(BParticlesSolver, BParticlesState *);
};

Description::~Description()
{
}
Solver::~Solver()
{
}

StateBase::~StateBase()
{
}

}  // namespace BParticles

struct Vector {
  float x, y, z;
};

using BParticles::Description;
using BParticles::Solver;
using BParticles::StateBase;
using BParticles::WrappedState;

WRAPPERS(BParticles::Description *, BParticlesDescription);
WRAPPERS(BParticles::Solver *, BParticlesSolver);
WRAPPERS(BParticles::WrappedState *, BParticlesState);

BParticlesDescription BParticles_playground_description()
{
  return wrap(new Description());
}
void BParticles_description_free(BParticlesDescription description_c)
{
  delete unwrap(description_c);
}

class SimpleSolver : public Solver {
 private:
  Description *m_description;

  struct MyState : StateBase {
    SmallVector<Vector> positions;
  };

 public:
  SimpleSolver(Description *description) : m_description(description)
  {
  }

  WrappedState *init() override
  {
    MyState *state = new MyState();
    return new WrappedState(this, std::unique_ptr<MyState>(state));
  }

  void step(WrappedState &wrapped_state) override
  {
    MyState &state = wrapped_state.state<MyState>();
    for (Vector &position : state.positions) {
      position.x += 0.1f;
    }
    state.positions.append({0, 0, 1});
  }

  uint particle_amount(WrappedState &wrapped_state) override
  {
    MyState &state = wrapped_state.state<MyState>();
    return state.positions.size();
  }

  void get_positions(WrappedState &wrapped_state, float (*dst)[3]) override
  {
    MyState &state = wrapped_state.state<MyState>();
    memcpy(dst, state.positions.begin(), state.positions.size() * sizeof(Vector));
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
  Solver *solver = unwrap(solver_c);
  return wrap(solver->init());
}
void BParticles_state_adapt(BParticlesSolver new_solver_c, BParticlesState *state_to_adapt_c)
{
  Solver *new_solver = unwrap(new_solver_c);
  WrappedState *wrapped_state = unwrap(*state_to_adapt_c);
  wrapped_state->m_solver = new_solver;
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
