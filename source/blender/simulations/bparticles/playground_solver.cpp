#include "BLI_small_vector.hpp"

#include "particles_container.hpp"
#include "playground_solver.hpp"

namespace BParticles {

class MoveUpAction : public BParticles::Action {
 public:
  void execute(AttributeArrays &buffers, ArrayRef<uint> indices_to_influence) override
  {
    auto positions = buffers.get_float3(buffers.info().attribute_index("Position"));

    for (uint i : indices_to_influence) {
      positions[i].z += 2.0f;
    }
  }
};

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
  AttributesInfo m_attributes;
  SmallVector<EmitterInfo> m_emitter_infos;

 public:
  SimpleSolver(Description &description) : m_description(description)
  {
    for (Emitter *emitter : m_description.emitters()) {
      EmitterInfoBuilder builder{emitter};
      emitter->info(builder);
      m_emitter_infos.append(builder.build());
    }

    SmallSetVector<std::string> byte_attributes = {"Kill State"};
    SmallSetVector<std::string> float_attributes = {"Birth Time"};
    SmallSetVector<std::string> float3_attributes;

    for (EmitterInfo &emitter : m_emitter_infos) {
      byte_attributes.add_multiple(emitter.used_byte_attributes());
      float_attributes.add_multiple(emitter.used_float_attributes());
      float3_attributes.add_multiple(emitter.used_float3_attributes());
    }

    m_attributes = AttributesInfo(
        byte_attributes.values(), float_attributes.values(), float3_attributes.values());
  }

  StateBase *init() override
  {
    MyState *state = new MyState();
    state->particles = new ParticlesContainer(m_attributes, 1000);
    return state;
  }

  BLI_NOINLINE void step_new_particles(AttributeArrays &slice, MyState &state)
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

  BLI_NOINLINE void step_slice(MyState &state, AttributeArrays &buffers, float elapsed_seconds)
  {
    auto positions = buffers.get_float3("Position");
    auto velocities = buffers.get_float3("Velocity");

    SmallVector<float3> combined_force(buffers.size());
    this->compute_combined_force(buffers, combined_force);

    SmallVector<float3> new_positions(buffers.size());
    SmallVector<float3> new_velocities(buffers.size());

    float mass = 1.0f;
    for (uint i = 0; i < buffers.size(); i++) {
      new_positions[i] = positions[i] + velocities[i] * elapsed_seconds;
      new_velocities[i] = velocities[i] + combined_force[i] / mass * elapsed_seconds;
    }

    SmallVector<uint> indices;

    for (uint i = 0; i < buffers.size(); i++) {
      if (positions[i].y <= 2 && new_positions[i].y > 2) {
        new_positions[i] = (positions[i] + new_positions[i]) * 0.5f;
        new_velocities[i] = (velocities[i] + new_velocities[i]) * 0.5f;
        indices.append(i);
      }
    }

    positions.copy_from(new_positions);
    velocities.copy_from(new_velocities);

    MoveUpAction action;
    action.execute(buffers, indices);

    auto birth_times = buffers.get_float("Birth Time");
    auto kill_states = buffers.get_byte("Kill State");

    for (uint i = 0; i < buffers.size(); i++) {
      float age = state.seconds_since_start - birth_times[i];
      if (age > 5) {
        kill_states[i] = 1;
      }
    }
  }

  BLI_NOINLINE void step_block(MyState &state, ParticlesBlock *block, float elapsed_seconds)
  {
    AttributeArrays slice = block->slice_active();
    this->step_slice(state, slice, elapsed_seconds);
  }

  BLI_NOINLINE void compute_combined_force(AttributeArrays &slice, ArrayRef<float3> dst)
  {
    BLI_assert(slice.size() == dst.size());
    dst.fill({0, 0, 0});
    for (Force *force : m_description.forces()) {
      force->add_force(slice, dst);
    }
  }

  BLI_NOINLINE void delete_dead_particles(ParticlesBlock *block)
  {
    auto kill_states = block->slice_active().get_byte("Kill State");

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
    SmallVector<EmitterBuffers> destinations;
    SmallVector<ParticlesBlock *> blocks;
    auto request_buffers = [&state, &destinations, &blocks]() -> EmitterBuffers & {
      ParticlesBlock *block = state.particles->new_block();
      blocks.append(block);
      destinations.append(EmitterBuffers{block->slice_all()});
      return destinations.last();
    };

    emitter.emitter().emit(request_buffers);

    for (uint i = 0; i < destinations.size(); i++) {
      EmitterBuffers &dst = destinations[i];
      ParticlesBlock *block = blocks[i];
      AttributeArrays emitted_data = dst.buffers().take_front(dst.emitted_amount());

      for (uint i : m_attributes.byte_attributes()) {
        if (!emitter.uses_byte_attribute(m_attributes.name_of(i))) {
          emitted_data.get_byte(i).fill(0);
        }
      }
      for (uint i : m_attributes.float_attributes()) {
        if (!emitter.uses_float_attribute(m_attributes.name_of(i))) {
          emitted_data.get_float(i).fill(0);
        }
      }
      for (uint i : m_attributes.float3_attributes()) {
        if (!emitter.uses_float3_attribute(m_attributes.name_of(i))) {
          emitted_data.get_float3(i).fill({0, 0, 0});
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
      auto positions = block->slice_active().get_float3("Position");
      memcpy(dst + index, positions.begin(), sizeof(float3) * positions.size());
      index += positions.size();
    }
  }
};

Solver *new_playground_solver(Description &description)
{
  return new SimpleSolver(description);
}

}  // namespace BParticles
