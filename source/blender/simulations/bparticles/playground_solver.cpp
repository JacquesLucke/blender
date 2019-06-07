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
    SmallVector<Vector> velocities;
  };

 public:
  SimpleSolver()
  {
  }

  StateBase *init() override
  {
    return new MyState();
  }

  void step(WrappedState &wrapped_state) override
  {
    MyState &state = wrapped_state.state<MyState>();

    for (uint i = 0; i < state.positions.size(); i++) {
      Vector &position = state.positions[i];
      Vector &velocity = state.velocities[i];
      position.x += velocity.x;
      position.y += velocity.y;
      position.z += velocity.z;
    }

    for (Vector &velocity : state.velocities) {
      velocity.z -= 0.001f;
    }

    state.positions.append({(float)(rand() % 100) / 100.0f, 0, 1});
    state.velocities.append({0, 0.1, 0});
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
