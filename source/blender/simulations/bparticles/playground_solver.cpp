#include "BLI_small_vector.hpp"

#include "particles_container.hpp"
#include "playground_solver.hpp"

namespace BParticles {

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
    SmallSetVector<std::string> float_attributes = {"Age"};
    SmallSetVector<std::string> vec3_attributes;

    for (Emitter *emitter : m_description.emitters()) {
      for (auto &name : emitter->used_float_attributes()) {
        float_attributes.add(name);
      }
      for (auto &name : emitter->used_vec3_attributes()) {
        vec3_attributes.add(name);
      }
    }

    MyState *state = new MyState();
    state->particles = new ParticlesContainer(
        10, float_attributes.values(), vec3_attributes.values());
    return state;
  }

  void step_block(ParticlesBlock *block, float elapsed_seconds)
  {
    uint active_amount = block->active_amount();

    Vec3 *positions = block->vec3_buffer("Position");
    Vec3 *velocities = block->vec3_buffer("Velocity");
    float *age = block->float_buffer("Age");

    for (uint i = 0; i < active_amount; i++) {
      positions[i] += velocities[i] * elapsed_seconds;
      age[i] += 1;
    }

    SmallVector<Vec3> combined_force(active_amount);
    combined_force.fill({0, 0, 0});

    ParticlesBlockSlice slice = block->slice_active();

    for (Force *force : m_description.forces()) {
      force->add_force(slice, combined_force);
    }

    for (uint i = 0; i < active_amount; i++) {
      velocities[i] += combined_force[i] * elapsed_seconds;
    }

    if (rand() % 10 == 0) {
      for (uint i = 0; i < active_amount; i++) {
        age[i] = rand() % 70;
      }
    }
  }

  void delete_old_particles(ParticlesBlock *block)
  {
    float *age = block->float_buffer("Age");

    uint index = 0;
    while (index < block->active_amount()) {
      if (age[index] < 50) {
        index++;
        continue;
      }
      if (age[block->active_amount() - 1] > 50) {
        block->active_amount() -= 1;
        continue;
      }
      block->move(block->active_amount() - 1, index);
      index++;
      block->active_amount() -= 1;
    }
  }

  void emit_new_particles(ParticlesContainer &particles)
  {
    for (Emitter *emitter : m_description.emitters()) {
      this->emit_from_emitter(particles, *emitter);
    }
  }

  void emit_from_emitter(ParticlesContainer &particles, Emitter &emitter)
  {
    SmallVector<ParticlesBlockSlice> block_slices;
    SmallVector<EmitterDestination> destinations;
    auto request_destination =
        [&particles, &block_slices, &destinations]() -> EmitterDestination & {
      ParticlesBlock *block = particles.new_block();
      block_slices.append(block->slice_all());
      destinations.append(EmitterDestination{block_slices.last()});
      return destinations.last();
    };

    emitter.emit(request_destination);

    for (uint i = 0; i < destinations.size(); i++) {
      ParticlesBlockSlice &slice = block_slices[i];
      EmitterDestination &dst = destinations[i];
      ParticlesBlock *block = slice.block();

      for (auto &name : particles.float_attribute_names()) {
        if (!emitter.used_float_attributes().contains(name)) {
          slice.float_buffer(name).fill(0);
        }
      }
      for (auto &name : particles.vec3_attribute_names()) {
        if (!emitter.used_vec3_attributes().contains(name)) {
          slice.vec3_buffer(name).fill(Vec3{0, 0, 0});
        }
      }

      block->active_amount() += dst.emitted_amount();
    }
  }

  void compress_all_blocks(ParticlesContainer &particles)
  {
    SmallVector<ParticlesBlock *> blocks;
    for (auto block : particles.active_blocks()) {
      blocks.append(block);
    }
    ParticlesBlock::Compress(blocks);

    for (auto block : blocks) {
      if (block->is_empty()) {
        particles.release_block(block);
      }
    }
  }

  void step(WrappedState &wrapped_state, float elapsed_seconds) override
  {
    MyState &state = wrapped_state.state<MyState>();

    ParticlesContainer &particles = *state.particles;

    for (ParticlesBlock *block : particles.active_blocks()) {
      this->step_block(block, elapsed_seconds);
      this->delete_old_particles(block);
    }

    this->emit_new_particles(particles);
    this->compress_all_blocks(particles);

    std::cout << "Particle Amount: " << this->particle_amount(wrapped_state) << "\n";
    std::cout << "Block amount: " << particles.active_blocks().size() << "\n";
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
