#include "BLI_small_vector.hpp"
#include "BLI_task.h"

#include "particles_container.hpp"
#include "playground_solver.hpp"

namespace BParticles {

class MoveUpAction : public BParticles::Action {
 public:
  void execute(AttributeArrays attributes, ArrayRef<uint> indices_mask) override
  {
    auto positions = attributes.get_float3("Position");

    for (uint i : indices_mask) {
      positions[i].y += 5.0f;
    }
  }
};

class HitPlaneEvent : public PositionalEvent {
 public:
  virtual void filter(AttributeArrays attributes,
                      ArrayRef<uint> indices_mask,
                      ArrayRef<float3> next_movement,
                      SmallVector<uint> &r_filtered_indices,
                      SmallVector<float> &r_time_factors) override
  {
    auto positions = attributes.get_float3("Position");
    for (uint i : indices_mask) {
      if (positions[i].y < 2.0f && positions[i].y + next_movement[i].y >= 2.0f) {
        float time_factor = (2.0f - positions[i].y) / next_movement[i].y;
        r_filtered_indices.append(i);
        r_time_factors.append(time_factor);
      }
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

  void step(WrappedState &wrapped_state, float elapsed_seconds) override
  {
    MyState &state = wrapped_state.state<MyState>();
    state.seconds_since_start += elapsed_seconds;

    ParticlesContainer &particles = *state.particles;

    SmallVector<ParticlesBlock *> already_existing_blocks =
        particles.active_blocks().to_small_vector();

    this->step_blocks(state, already_existing_blocks, elapsed_seconds);
    this->delete_dead_particles(already_existing_blocks);
    this->emit_new_particles(state, elapsed_seconds);
    this->compress_all_blocks(particles);

    std::cout << "Particle Amount: " << this->particle_amount(wrapped_state) << "\n";
    std::cout << "Block amount: " << particles.active_blocks().size() << "\n";
  }

  struct StepBlocksParallelData {
    SimpleSolver *solver;
    MyState &state;
    ArrayRef<ParticlesBlock *> blocks;
    float elapsed_seconds;
  };

  BLI_NOINLINE void step_blocks(MyState &state,
                                ArrayRef<ParticlesBlock *> blocks,
                                float elapsed_seconds)
  {
    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);

    StepBlocksParallelData data = {this, state, blocks, elapsed_seconds};

    BLI_task_parallel_range(0, blocks.size(), (void *)&data, step_block_cb, &settings);
  }

  static void step_block_cb(void *__restrict userdata,
                            const int index,
                            const ParallelRangeTLS *__restrict UNUSED(tls))
  {
    StepBlocksParallelData *data = (StepBlocksParallelData *)userdata;
    ParticlesBlock *block = data->blocks[index];
    AttributeArrays attributes = block->slice_active();

    data->solver->step_slice(data->state,
                             attributes,
                             Range<uint>(0, attributes.size()).to_small_vector(),
                             data->elapsed_seconds);
  }

  BLI_NOINLINE void step_slice(MyState &state,
                               AttributeArrays attributes,
                               ArrayRef<uint> indices_mask,
                               float elapsed_seconds)
  {
    SmallVector<float> time_diffs(attributes.size());
    time_diffs.fill(elapsed_seconds);

    SmallVector<float3> position_offsets(attributes.size());
    SmallVector<float3> velocity_offsets(attributes.size());

    this->integrate_particles(
        attributes, indices_mask, time_diffs, position_offsets, velocity_offsets);

    auto positions = attributes.get_float3("Position");
    auto velocities = attributes.get_float3("Velocity");

    HitPlaneEvent event;
    SmallVector<uint> triggered_indices;
    SmallVector<float> triggered_time_factors;
    event.filter(
        attributes, indices_mask, position_offsets, triggered_indices, triggered_time_factors);

    MoveUpAction action;
    action.execute(attributes, triggered_indices);

    auto birth_times = attributes.get_float("Birth Time");
    auto kill_states = attributes.get_byte("Kill State");

    for (uint i : indices_mask) {
      float age = state.seconds_since_start - birth_times[i];
      if (age > 5) {
        kill_states[i] = 1;
      }
    }

    for (uint i : indices_mask) {
      positions[i] += position_offsets[i];
      velocities[i] += velocity_offsets[i];
    }
  }

  BLI_NOINLINE void integrate_particles(AttributeArrays attributes,
                                        ArrayRef<uint> indices_mask,
                                        ArrayRef<float> time_diffs,
                                        ArrayRef<float3> r_position_offsets,
                                        ArrayRef<float3> r_velocity_offsets)
  {
    BLI_assert(attributes.size() == time_diffs.size());

    SmallVector<float3> combined_force(attributes.size());
    this->compute_combined_force(attributes, indices_mask, combined_force);

    auto velocities = attributes.get_float3("Velocity");

    for (uint i : indices_mask) {
      float mass = 1.0f;
      float time_diff = time_diffs[i];
      r_velocity_offsets[i] = time_diff * combined_force[i] / mass;
      r_position_offsets[i] = time_diff * (velocities[i] + r_velocity_offsets[i] * 0.5f);
    }
  }

  BLI_NOINLINE void compute_combined_force(AttributeArrays attributes,
                                           ArrayRef<uint> indices_mask,
                                           ArrayRef<float3> dst)
  {
    BLI_assert(indices_mask.size() == dst.size());
    dst.fill({0, 0, 0});
    for (Force *force : m_description.forces()) {
      force->add_force(attributes, indices_mask, dst);
    }
  }

  BLI_NOINLINE void step_new_particles(AttributeArrays attributes, MyState &state)
  {
    auto positions = attributes.get_float3("Position");
    auto velocities = attributes.get_float3("Velocity");
    auto birth_times = attributes.get_float("Birth Time");

    SmallVector<float3> combined_force(attributes.size());
    this->compute_combined_force(
        attributes, Range<uint>(0, attributes.size()).to_small_vector(), combined_force);

    for (uint i = 0; i < attributes.size(); i++) {
      float seconds_since_birth = state.seconds_since_start - birth_times[i];
      positions[i] += velocities[i] * seconds_since_birth;
      velocities[i] += combined_force[i] * seconds_since_birth;
    }
  }

  BLI_NOINLINE void delete_dead_particles(ArrayRef<ParticlesBlock *> blocks)
  {
    for (auto block : blocks) {
      this->delete_dead_particles(block);
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
    SmallVector<EmitterTarget> targets;
    SmallVector<ParticlesBlock *> blocks;

    RequestEmitterTarget request_target = [&state, &targets, &blocks]() -> EmitterTarget & {
      ParticlesBlock *block = state.particles->new_block();
      blocks.append(block);
      targets.append(EmitterTarget{block->slice_all()});
      return targets.last();
    };

    emitter.emitter().emit(EmitterHelper{request_target});

    for (uint i = 0; i < targets.size(); i++) {
      EmitterTarget &target = targets[i];
      ParticlesBlock *block = blocks[i];
      AttributeArrays emitted_data = target.attributes().take_front(target.emitted_amount());

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
      block->active_amount() += target.emitted_amount();
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

  /* Access data from the outside.
   *********************************************/

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
