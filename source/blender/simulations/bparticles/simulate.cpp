#include "simulate.hpp"
#include "time_span.hpp"
#include "BLI_lazy_init.hpp"

namespace BParticles {

/* Static Data
 **************************************************/

BLI_LAZY_INIT_STATIC(SmallVector<uint>, static_number_range_vector)
{
  return Range<uint>(0, 10000).to_small_vector();
}

static ArrayRef<uint> static_number_range_ref()
{
  return static_number_range_vector();
}

/* Evaluate Forces
 ***********************************************/

static void compute_combined_forces_on_particles(AttributeArrays attributes,
                                                 ArrayRef<uint> particle_indices,
                                                 ArrayRef<Force *> forces,
                                                 ArrayRef<float3> r_force_vectors)
{
  BLI_assert(particle_indices.size() == r_force_vectors.size());
  r_force_vectors.fill({0, 0, 0});
  for (Force *force : forces) {
    force->add_force(attributes, particle_indices, r_force_vectors);
  }
}

/* Step individual particles.
 **********************************************/

static void compute_ideal_attribute_offsets(AttributeArrays attributes,
                                            ArrayRef<uint> particle_indices,
                                            ArrayRef<float> durations,
                                            ParticleInfluences &influences,
                                            ArrayRef<float3> r_position_offsets,
                                            ArrayRef<float3> r_velocity_offsets)
{
  BLI_assert(particle_indices.size() == durations.size());
  BLI_assert(particle_indices.size() == r_position_offsets.size());
  BLI_assert(particle_indices.size() == r_velocity_offsets.size());

  SmallVector<float3> combined_force{particle_indices.size()};
  compute_combined_forces_on_particles(
      attributes, particle_indices, influences.forces(), combined_force);

  auto velocities = attributes.get_float3("Velocity");

  for (uint i = 0; i < particle_indices.size(); i++) {
    uint pindex = particle_indices[i];

    float mass = 1.0f;
    float duration = durations[i];

    r_velocity_offsets[i] = duration * combined_force[i] / mass;
    r_position_offsets[i] = duration * (velocities[pindex] + r_velocity_offsets[i] * 0.5f);
  }
}

static void step_individual_particles(AttributeArrays attributes,
                                      ArrayRef<uint> particle_indices,
                                      ArrayRef<float> durations,
                                      ParticleInfluences &influences)
{
  SmallVector<float3> position_offsets{particle_indices.size()};
  SmallVector<float3> velocity_offsets{particle_indices.size()};

  compute_ideal_attribute_offsets(
      attributes, particle_indices, durations, influences, position_offsets, velocity_offsets);

  auto positions = attributes.get_float3("Position");
  auto velocities = attributes.get_float3("Velocity");

  for (uint i = 0; i < particle_indices.size(); i++) {
    uint pindex = particle_indices[i];

    positions[pindex] += position_offsets[i];
    velocities[pindex] += velocity_offsets[i];
  }
}

/* Emit new particles from emitters.
 **********************************************/

static void emit_new_particles_from_emitter(ParticlesContainer &container,
                                            Emitter &emitter,
                                            ParticleInfluences &influences,
                                            TimeSpan time_span)
{
  SmallVector<EmitterTarget> targets;
  SmallVector<ParticlesBlock *> blocks;

  RequestEmitterTarget request_target = [&container, &targets, &blocks]() -> EmitterTarget & {
    ParticlesBlock *block = container.new_block();
    blocks.append(block);
    targets.append(EmitterTarget{block->slice_all()});
    return targets.last();
  };

  emitter.emit(EmitterHelper{request_target});

  for (uint i = 0; i < targets.size(); i++) {
    EmitterTarget &target = targets[i];
    ParticlesBlock *block = blocks[i];
    AttributeArrays emitted_attributes = target.attributes().take_front(target.emitted_amount());

    emitted_attributes.get_byte("Kill State").fill(0);

    auto birth_times = emitted_attributes.get_float("Birth Time");
    for (float &birth_time : birth_times) {
      float fac = (rand() % 1000) / 1000.0f;
      birth_time = time_span.interpolate(fac);
    }

    SmallVector<float> initial_step_durations;
    for (float birth_time : birth_times) {
      initial_step_durations.append(time_span.end() - birth_time);
    }

    block->active_amount() += target.emitted_amount();
    step_individual_particles(emitted_attributes,
                              Range<uint>(0, emitted_attributes.size()).to_small_vector(),
                              initial_step_durations,
                              influences);
  }
}

static void emit_new_particles_from_emitters(ParticlesContainer &container,
                                             ArrayRef<Emitter *> emitters,
                                             ParticleInfluences &influences,
                                             TimeSpan time_span)
{
  for (Emitter *emitter : emitters) {
    emit_new_particles_from_emitter(container, *emitter, influences, time_span);
  }
}

/* Compress particle blocks.
 **************************************************/

static void compress_all_blocks(ParticlesContainer &particles)
{
  SmallVector<ParticlesBlock *> blocks = particles.active_blocks().to_small_vector();
  ParticlesBlock::Compress(blocks);

  for (ParticlesBlock *block : blocks) {
    if (block->is_empty()) {
      particles.release_block(block);
    }
  }
}

/* Main Entry Point
 **************************************************/

void simulate_step(ParticlesState &state, StepDescription &description)
{
  TimeSpan time_span{state.m_current_time, description.step_duration()};
  state.m_current_time = time_span.end();

  ParticlesContainer &particles = *state.m_container;

  SmallVector<ParticlesBlock *> already_existing_blocks =
      particles.active_blocks().to_small_vector();

  SmallVector<float> durations_vector(particles.block_size());
  durations_vector.fill(time_span.duration());
  ArrayRef<float> durations = durations_vector;

  for (ParticlesBlock *block : already_existing_blocks) {
    step_individual_particles(block->slice_active(),
                              static_number_range_ref().take_front(block->active_amount()),
                              durations.take_front(block->active_amount()),
                              description.influences());
  }

  emit_new_particles_from_emitters(
      particles, description.emitters(), description.influences(), time_span);

  compress_all_blocks(particles);
}

}  // namespace BParticles
