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

/* Events
 **************************************************/

static void find_next_event_per_particle(AttributeArrays attributes,
                                         ArrayRef<uint> particle_indices,
                                         IdealOffsets &ideal_offsets,
                                         ArrayRef<float> durations,
                                         ArrayRef<Event *> events,
                                         ArrayRef<int> r_next_event_indices,
                                         ArrayRef<float> r_time_factors_to_next_event)
{
  r_next_event_indices.fill(-1);
  r_time_factors_to_next_event.fill(1.0f);

  for (uint event_index = 0; event_index < events.size(); event_index++) {
    SmallVector<uint> triggered_indices;
    SmallVector<float> triggered_time_factors;

    Event *event = events[event_index];
    event->filter(
        attributes, particle_indices, ideal_offsets, triggered_indices, triggered_time_factors);

    for (uint i = 0; i < triggered_indices.size(); i++) {
      uint index = triggered_indices[i];
      if (triggered_time_factors[i] < r_time_factors_to_next_event[index]) {
        r_next_event_indices[index] = event_index;
        r_time_factors_to_next_event[index] = triggered_time_factors[i];
      }
    }
  }
}

static void forward_particles_to_next_event(AttributeArrays attributes,
                                            ArrayRef<uint> particle_indices,
                                            IdealOffsets &ideal_offsets,
                                            ArrayRef<float> time_factors_to_next_event)
{
  auto positions = attributes.get_float3("Position");
  auto velocities = attributes.get_float3("Velocity");

  for (uint i = 0; i < particle_indices.size(); i++) {
    uint pindex = particle_indices[i];
    float time_factor = time_factors_to_next_event[i];
    positions[pindex] += time_factor * ideal_offsets.position_offsets[i];
    velocities[pindex] += time_factor * ideal_offsets.velocity_offsets[i];
  }
}

static void find_particles_per_event(ArrayRef<uint> particle_indices,
                                     ArrayRef<int> next_event_indices,
                                     ArrayRef<SmallVector<uint>> r_particles_per_event)
{
  for (uint i = 0; i < particle_indices.size(); i++) {
    int event_index = next_event_indices[i];
    if (event_index != -1) {
      uint pindex = particle_indices[i];
      r_particles_per_event[event_index].append(pindex);
    }
  }
}

static void find_unfinished_particles(ArrayRef<uint> particle_indices,
                                      ArrayRef<int> next_event_indices,
                                      ArrayRef<float> time_factors_to_next_event,
                                      ArrayRef<float> durations,
                                      SmallVector<uint> &r_unfinished_particle_indices,
                                      SmallVector<float> &r_remaining_durations)
{
  for (uint i = 0; i < particle_indices.size(); i++) {
    uint pindex = particle_indices[i];
    if (next_event_indices[i] != -1) {
      float time_factor = time_factors_to_next_event[i];
      float remaining_duration = durations[i] * (1.0f - time_factor);

      r_unfinished_particle_indices.append(pindex);
      r_remaining_durations.append(remaining_duration);
    }
  }
}

static void run_actions(AttributeArrays attributes,
                        ArrayRef<SmallVector<uint>> particles_per_event,
                        ArrayRef<Event *> events,
                        ArrayRef<Action *> action_per_event)
{
  for (uint event_index = 0; event_index < events.size(); event_index++) {
    Action *action = action_per_event[event_index];
    action->execute(attributes, particles_per_event[event_index]);
  }
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
                                            IdealOffsets r_offsets)
{
  BLI_assert(particle_indices.size() == durations.size());
  BLI_assert(particle_indices.size() == r_offsets.position_offsets.size());
  BLI_assert(particle_indices.size() == r_offsets.velocity_offsets.size());

  SmallVector<float3> combined_force{particle_indices.size()};
  compute_combined_forces_on_particles(
      attributes, particle_indices, influences.forces(), combined_force);

  auto velocities = attributes.get_float3("Velocity");

  for (uint i = 0; i < particle_indices.size(); i++) {
    uint pindex = particle_indices[i];

    float mass = 1.0f;
    float duration = durations[i];

    r_offsets.velocity_offsets[i] = duration * combined_force[i] / mass;
    r_offsets.position_offsets[i] = duration *
                                    (velocities[pindex] + r_offsets.velocity_offsets[i] * 0.5f);
  }
}

static void simulate_to_next_event(AttributeArrays attributes,
                                   ArrayRef<uint> particle_indices,
                                   ArrayRef<float> durations,
                                   ParticleInfluences &influences,
                                   SmallVector<uint> &r_unfinished_particle_indices,
                                   SmallVector<float> &r_remaining_durations)
{
  SmallVector<float3> position_offsets(particle_indices.size());
  SmallVector<float3> velocity_offsets(particle_indices.size());
  IdealOffsets ideal_offsets{position_offsets, velocity_offsets};

  compute_ideal_attribute_offsets(
      attributes, particle_indices, durations, influences, ideal_offsets);

  SmallVector<int> next_event_indices(particle_indices.size());
  SmallVector<float> time_factors_to_next_event(particle_indices.size());

  find_next_event_per_particle(attributes,
                               particle_indices,
                               ideal_offsets,
                               durations,
                               influences.events(),
                               next_event_indices,
                               time_factors_to_next_event);

  forward_particles_to_next_event(
      attributes, particle_indices, ideal_offsets, time_factors_to_next_event);

  SmallVector<SmallVector<uint>> particles_per_event(influences.events().size());
  find_particles_per_event(particle_indices, next_event_indices, particles_per_event);
  run_actions(attributes, particles_per_event, influences.events(), influences.action_per_event());

  find_unfinished_particles(particle_indices,
                            next_event_indices,
                            time_factors_to_next_event,
                            durations,
                            r_unfinished_particle_indices,
                            r_remaining_durations);
}

static void simulate_ignoring_events(AttributeArrays attributes,
                                     ArrayRef<uint> particle_indices,
                                     ArrayRef<float> durations,
                                     ParticleInfluences &influences)
{
  SmallVector<float3> position_offsets{particle_indices.size()};
  SmallVector<float3> velocity_offsets{particle_indices.size()};
  IdealOffsets offsets{position_offsets, velocity_offsets};

  compute_ideal_attribute_offsets(attributes, particle_indices, durations, influences, offsets);

  auto positions = attributes.get_float3("Position");
  auto velocities = attributes.get_float3("Velocity");

  for (uint i = 0; i < particle_indices.size(); i++) {
    uint pindex = particle_indices[i];

    positions[pindex] += offsets.position_offsets[i];
    velocities[pindex] += offsets.velocity_offsets[i];
  }
}

static void step_individual_particles(AttributeArrays attributes,
                                      ArrayRef<uint> particle_indices,
                                      ArrayRef<float> durations,
                                      ParticleInfluences &influences)
{
  SmallVector<uint> unfinished_particle_indices;
  SmallVector<float> remaining_durations;
  simulate_to_next_event(attributes,
                         particle_indices,
                         durations,
                         influences,
                         unfinished_particle_indices,
                         remaining_durations);
  BLI_assert(unfinished_particle_indices.size() == remaining_durations.size());

  simulate_ignoring_events(
      attributes, unfinished_particle_indices, remaining_durations, influences);
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
