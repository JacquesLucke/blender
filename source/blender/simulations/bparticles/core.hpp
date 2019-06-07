#pragma once

#include <memory>

#include "BLI_utildefines.h"

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

  friend void adapt_state(Solver *, WrappedState *);
};

void adapt_state(Solver *new_solver, WrappedState *wrapped_state);

}  // namespace BParticles
