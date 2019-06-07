#include "BLI_small_vector.hpp"

#include "playground_solver.hpp"

namespace BParticles {

using BLI::SmallVector;

struct Vector {
  float x, y, z;
};

class SimpleSolver : public Solver {

  struct MyState : StateBase {
    SmallVector<Vector> positions;
  };

 public:
  SimpleSolver()
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

Solver *new_playground_solver()
{
  return new SimpleSolver();
}

}  // namespace BParticles
