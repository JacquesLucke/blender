#include "BLI_small_vector.hpp"

#include "particles_container.hpp"
#include "playground_solver.hpp"

namespace BParticles {

using BLI::SmallVector;

struct Vector {
  float x, y, z;
};

class SimpleSolver : public Solver {

  struct MyState : StateBase {
    ParticlesContainer *particles;

    ~MyState()
    {
      delete particles;
    }
  };

  Description &m_description;

 public:
  SimpleSolver(Description &description) : m_description(description)
  {
  }

  StateBase *init() override
  {
    MyState *state = new MyState();
    state->particles = new ParticlesContainer(50, {}, {"Position", "Velocity"});
    return state;
  }

  void step_block(ParticlesBlock *block)
  {
    uint active_amount = block->active_amount();

    Vec3 *positions = block->vec3_buffer("Position");
    Vec3 *velocities = block->vec3_buffer("Velocity");

    for (uint i = 0; i < active_amount; i++) {
      positions[i] += velocities[i];
    }

    SmallVector<Vec3> combined_force(active_amount);
    combined_force.fill({0, 0, 0});

    for (Force *force : m_description.forces()) {
      force->add_force(combined_force);
    }

    float time_step = 0.01f;
    for (uint i = 0; i < active_amount; i++) {
      velocities[i] += combined_force[i] * time_step;
    }
  }

  void emit_new_particles(ParticlesContainer &particles)
  {
    ParticlesBlock *non_full_block = nullptr;
    for (ParticlesBlock *block : particles.active_blocks()) {
      if (!block->is_full()) {
        non_full_block = block;
        break;
      }
    }

    if (non_full_block == nullptr) {
      non_full_block = particles.new_block();
    }

    uint index = non_full_block->next_inactive_index();
    non_full_block->vec3_buffer("Position")[index] = {(float)(rand() % 100) / 100.0f, 0, 1};
    non_full_block->vec3_buffer("Velocity")[index] = {0, 0.1, 0};
    non_full_block->active_amount()++;
  }

  void step(WrappedState &wrapped_state) override
  {
    MyState &state = wrapped_state.state<MyState>();

    ParticlesContainer &particles = *state.particles;

    for (ParticlesBlock *block : particles.active_blocks()) {
      this->step_block(block);
    }

    this->emit_new_particles(particles);
  }

  uint particle_amount(WrappedState &wrapped_state) override
  {
    MyState &state = wrapped_state.state<MyState>();

    uint count = 0;
    for (auto *block : state.particles->active_blocks()) {
      count += block->active_amount();
    }

    return count;
  }

  void get_positions(WrappedState &wrapped_state, float (*dst)[3]) override
  {
    MyState &state = wrapped_state.state<MyState>();

    uint index = 0;
    for (auto *block : state.particles->active_blocks()) {
      uint length = block->active_amount();
      memcpy(dst + index, block->vec3_buffer("Position"), sizeof(Vec3) * length);
      index += length;
    }
  }
};

Solver *new_playground_solver(Description &description)
{
  return new SimpleSolver(description);
}

}  // namespace BParticles
