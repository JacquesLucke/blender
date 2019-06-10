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
    SmallSetVector<std::string> byte_attributes = {"Kill State"};

    for (EmitterInfo &emitter : m_emitter_infos) {
      float_attributes.add_multiple(emitter.used_float_attributes());
      vec3_attributes.add_multiple(emitter.used_vec3_attributes());
      byte_attributes.add_multiple(emitter.used_byte_attributes());
    }

    MyState *state = new MyState();
    state->particles = new ParticlesContainer(
        1000, float_attributes.values(), vec3_attributes.values(), byte_attributes.values());
    return state;
  }

  BLI_NOINLINE void step_new_particles(NamedBuffers &slice, MyState &state)
  {
    auto positions = slice.get_float3("Position");
    auto velocities = slice.get_float3("Velocity");
    auto birth_times = slice.get_float("Birth Time");

    SmallVector<float3> combined_force(slice.size());
    this->compute_combined_force(slice, combined_force);

    for (uint i = 0; i < slice.size(); i++) {
      float seconds_since_birth = state.seconds_since_start - birth_times[i];
      positions[i] += velocities[i] * seconds_since_birth;
      velocities[i] += combined_force[i] * seconds_since_birth;
    }
  }

  BLI_NOINLINE void step_slice(MyState &state, NamedBuffers &slice, float elapsed_seconds)
  {
    auto positions = slice.get_float3("Position");
    auto velocities = slice.get_float3("Velocity");

    SmallVector<float3> combined_force(slice.size());
    this->compute_combined_force(slice, combined_force);

    for (uint i = 0; i < slice.size(); i++) {
      positions[i] += velocities[i] * elapsed_seconds;
      velocities[i] += combined_force[i] * elapsed_seconds;
    }

    auto birth_times = slice.get_float("Birth Time");
    auto kill_states = slice.get_byte("Kill State");

    for (uint i = 0; i < slice.size(); i++) {
      float age = state.seconds_since_start - birth_times[i];
      if (age > 5) {
        kill_states[i] = 1;
      }
    }
  }

  BLI_NOINLINE void step_block(MyState &state, ParticlesBlock *block, float elapsed_seconds)
  {
    ParticlesBlockSlice slice = block->slice_active();
    this->step_slice(state, slice, elapsed_seconds);
  }

  BLI_NOINLINE void compute_combined_force(NamedBuffers &slice, ArrayRef<float3> dst)
  {
    BLI_assert(slice.size() == dst.size());
    dst.fill({0, 0, 0});
    for (Force *force : m_description.forces()) {
      force->add_force(slice, dst);
    }
  }

  BLI_NOINLINE void delete_dead_particles(ParticlesBlock *block)
  {
    uint8_t *kill_states = block->byte_buffer("Kill State");

    uint index = 0;
    while (index < block->active_amount()) {
      if (kill_states[index] == 1) {
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
    SmallVector<EmitterBuffers> destinations;
    auto request_buffers = [&state, &block_slices, &destinations]() -> EmitterBuffers & {
      ParticlesBlock *block = state.particles->new_block();
      block_slices.append(block->slice_all());
      destinations.append(EmitterBuffers{block_slices.last()});
      return destinations.last();
    };

    emitter.emitter().emit(request_buffers);

    for (uint i = 0; i < destinations.size(); i++) {
      EmitterBuffers &dst = destinations[i];
      ParticlesBlockSlice emitted_data = block_slices[i].take_front(dst.emitted_amount());
      ParticlesBlock *block = emitted_data.block();

      for (auto &name : state.particles->float_attribute_names()) {
        if (!emitter.uses_float_attribute(name)) {
          emitted_data.get_float(name).fill(0);
        }
      }
      for (auto &name : state.particles->vec3_attribute_names()) {
        if (!emitter.uses_vec3_attribute(name)) {
          emitted_data.get_float3(name).fill(float3{0, 0, 0});
        }
      }

      auto birth_times = emitted_data.get_float("Birth Time");
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
      this->step_block(state, block, elapsed_seconds);
      this->delete_dead_particles(block);
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
      memcpy(dst + index, block->float3_buffer("Position"), sizeof(float3) * length);
      index += length;
    }
  }
};

Solver *new_playground_solver(Description &description)
{
  return new SimpleSolver(description);
}

}  // namespace BParticles
