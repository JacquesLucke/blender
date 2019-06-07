#include "BLI_small_vector.hpp"

#include "playground_solver.hpp"

namespace BParticles {

using BLI::SmallVector;

struct Vector {
  float x, y, z;
};

class SimpleSolver : public Solver {

  struct MyState : StateBase {
    SmallVector<Vec3> positions;
    SmallVector<Vec3> velocities;
  };

  Description &m_description;

 public:
  SimpleSolver(Description &description) : m_description(description)
  {
  }

  StateBase *init() override
  {
    return new MyState();
  }

  void step(WrappedState &wrapped_state) override
  {
    MyState &state = wrapped_state.state<MyState>();
    uint last_particle_amount = state.positions.size();

    for (uint i = 0; i < last_particle_amount; i++) {
      Vec3 &position = state.positions[i];
      Vec3 &velocity = state.velocities[i];
      position.x += velocity.x;
      position.y += velocity.y;
      position.z += velocity.z;
    }

    SmallVector<Vec3> combined_force(last_particle_amount);
    combined_force.fill({0, 0, 0});

    for (Force *force : m_description.forces()) {
      force->add_force(combined_force);
    }

    float time_step = 0.01f;
    for (uint i = 0; i < last_particle_amount; i++) {
      state.velocities[i] += combined_force[i] * time_step;
    }

    for (uint i = 0; i < last_particle_amount; i++) {
      state.positions[i] += state.velocities[i] * time_step;
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

Solver *new_playground_solver(Description &description)
{
  return new SimpleSolver(description);
}

}  // namespace BParticles
