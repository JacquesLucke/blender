#include "core.hpp"

namespace BParticles {

Description::~Description()
{
}

Solver::~Solver()
{
}

StateBase::~StateBase()
{
}

void adapt_state(Solver *new_solver, WrappedState *wrapped_state)
{
  wrapped_state->m_solver = new_solver;
}

}  // namespace BParticles
