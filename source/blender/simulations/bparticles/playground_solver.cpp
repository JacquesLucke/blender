#include "BLI_small_vector.hpp"

#include "particles_container.hpp"
#include "playground_solver.hpp"

namespace BParticles {

class SimpleSolver : public Solver {

  struct MyState : StateBase {
    ParticlesContainer *particles;
    float seconds_since_start = 0.0f;

    ~MyState()
    {
      delete particles;
    }
  };

  Description &m_description;
  SmallVector<EmitterInfo> m_emitter_infos;

 public:
  SimpleSolver(Description &description) : m_description(description)
  {
    for (Emitter *emitter : m_description.emitters()) {
      EmitterInfoBuilder builder{emitter};
      emitter->info(builder);
      m_emitter_infos.append(builder.build());
    }
  }

  StateBase *init() override
  {
    SmallSetVector<std::string> float_attributes = {"Birth Time"};
    SmallSetVector<std::string> vec3_attributes;

    for (EmitterInfo &emitter : m_emitter_infos) {
      float_attributes.add_multiple(emitter.used_float_attributes());
      vec3_attributes.add_multiple(emitter.used_vec3_attributes());
    }

    MyState *state = new MyState();
    state->particles = new ParticlesContainer(
        1000, float_attributes.values(), vec3_attributes.values());
    return state;
  }

  BLI_NOINLINE void step_new_particles(ParticlesBlockSlice slice, MyState &state)
  {
    auto positions = slice.vec3_buffer("Position");
    auto velocities = slice.vec3_buffer("Velocity");
    auto birth_times = slice.float_buffer("Birth Time");

    SmallVector<Vec3> combined_force(slice.size());
    this->compute_combined_force(slice, combined_force);

    for (uint i = 0; i < slice.size(); i++) {
      float seconds_since_birth = state.seconds_since_start - birth_times[i];
      positions[i] += velocities[i] * seconds_since_birth;
      velocities[i] += combined_force[i] * seconds_since_birth;
    }
  }

  BLI_NOINLINE void step_block(ParticlesBlock *block, float elapsed_seconds)
  {
    uint active_amount = block->active_amount();

    Vec3 *positions = block->vec3_buffer("Position");
    Vec3 *velocities = block->vec3_buffer("Velocity");

    for (uint i = 0; i < active_amount; i++) {
      positions[i] += velocities[i] * elapsed_seconds;
    }

    ParticlesBlockSlice slice = block->slice_active();
    SmallVector<Vec3> combined_force(active_amount);
    this->compute_combined_force(slice, combined_force);

    for (uint i = 0; i < active_amount; i++) {
      velocities[i] += combined_force[i] * elapsed_seconds;
    }
  }

  BLI_NOINLINE void compute_combined_force(ParticlesBlockSlice &slice, ArrayRef<Vec3> dst)
  {
    BLI_assert(slice.size() == dst.size());
    dst.fill({0, 0, 0});
    for (Force *force : m_description.forces()) {
      force->add_force(slice, dst);
    }
  }

  BLI_NOINLINE void delete_old_particles(MyState &state, ParticlesBlock *block)
  {
    float *birth_time = block->float_buffer("Birth Time");

    uint index = 0;
    while (index < block->active_amount()) {
      if (state.seconds_since_start - 3 > birth_time[index]) {
        block->move(block->active_amount() - 1, index);
        block->active_amount() -= 1;
      }
      else {
        index++;
      }
    }
  }

  BLI_NOINLINE void emit_new_particles(MyState &state, float elapsed_seconds)
  {
    for (EmitterInfo &emitter : m_emitter_infos) {
      this->emit_from_emitter(state, emitter, elapsed_seconds);
    }
  }

  void emit_from_emitter(MyState &state, EmitterInfo &emitter, float elapsed_seconds)
  {
    SmallVector<ParticlesBlockSlice> block_slices;
    SmallVector<EmitterDestination> destinations;
    auto request_destination = [&state, &block_slices, &destinations]() -> EmitterDestination & {
      ParticlesBlock *block = state.particles->new_block();
      block_slices.append(block->slice_all());
      destinations.append(EmitterDestination{block_slices.last()});
      return destinations.last();
    };

    emitter.emitter().emit(request_destination);

    for (uint i = 0; i < destinations.size(); i++) {
      EmitterDestination &dst = destinations[i];
      ParticlesBlockSlice emitted_data = block_slices[i].take_front(dst.emitted_amount());
      ParticlesBlock *block = emitted_data.block();

      for (auto &name : state.particles->float_attribute_names()) {
        if (!emitter.uses_float_attribute(name)) {
          emitted_data.float_buffer(name).fill(0);
        }
      }
      for (auto &name : state.particles->vec3_attribute_names()) {
        if (!emitter.uses_vec3_attribute(name)) {
          emitted_data.vec3_buffer(name).fill(Vec3{0, 0, 0});
        }
      }

      auto birth_times = emitted_data.float_buffer("Birth Time");
      for (float &birth_time : birth_times) {
        float fac = (rand() % 1000) / 1000.0f;
        birth_time = state.seconds_since_start - elapsed_seconds * fac;
      }
      block->active_amount() += dst.emitted_amount();
      this->step_new_particles(emitted_data, state);
    }
  }

  BLI_NOINLINE void compress_all_blocks(ParticlesContainer &particles)
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
    state.seconds_since_start += elapsed_seconds;

    ParticlesContainer &particles = *state.particles;

    for (ParticlesBlock *block : particles.active_blocks()) {
      this->step_block(block, elapsed_seconds);
      this->delete_old_particles(state, block);
    }

    this->emit_new_particles(state, elapsed_seconds);
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
