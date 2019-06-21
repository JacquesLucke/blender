#include "simulate.hpp"
#include "time_span.hpp"

#include "BLI_lazy_init.hpp"
#include "BLI_task.h"

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

BLI_NOINLINE static void find_next_event_per_particle(ParticleSet particles,
                                                      IdealOffsets &ideal_offsets,
                                                      ArrayRef<float> durations,
                                                      float end_time,
                                                      ArrayRef<Event *> events,
                                                      ArrayRef<float> last_event_times,
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
        particles, ideal_offsets, durations, end_time, triggered_indices, triggered_time_factors);

    for (uint i = 0; i < triggered_indices.size(); i++) {
      uint index = triggered_indices[i];
      float time_factor = triggered_time_factors[i];
      if (time_factor < r_time_factors_to_next_event[index]) {
        if (last_event_times.size() > 0) {
          float trigger_time = end_time - durations[index] * (1.0f - time_factor);
          if (trigger_time - last_event_times[index] < 0.00001) {
            continue;
          }
        }
        r_next_event_indices[index] = event_index;
        r_time_factors_to_next_event[index] = time_factor;
      }
    }
  }
}

BLI_NOINLINE static void forward_particles_to_next_event(
    ParticleSet particles, IdealOffsets &ideal_offsets, ArrayRef<float> time_factors_to_next_event)
{
  auto positions = particles.attributes().get_float3("Position");
  auto velocities = particles.attributes().get_float3("Velocity");

  for (uint i : particles.range()) {
    uint pindex = particles.pindex_of(i);
    float time_factor = time_factors_to_next_event[i];
    positions[pindex] += time_factor * ideal_offsets.position_offsets[i];
    velocities[pindex] += time_factor * ideal_offsets.velocity_offsets[i];
  }
}

BLI_NOINLINE static void find_particles_per_event(
    ArrayRef<uint> particle_indices,
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

BLI_NOINLINE static void find_unfinished_particles(
    ArrayRef<uint> particle_indices,
    ArrayRef<int> next_event_indices,
    ArrayRef<float> time_factors_to_next_event,
    ArrayRef<float> durations,
    ArrayRef<uint8_t> kill_states,
    SmallVector<uint> &r_unfinished_particle_indices,
    SmallVector<float> &r_remaining_durations)
{

  for (uint i = 0; i < particle_indices.size(); i++) {
    uint pindex = particle_indices[i];
    if (next_event_indices[i] != -1 && kill_states[pindex] == 0) {
      float time_factor = time_factors_to_next_event[i];
      float remaining_duration = durations[i] * (1.0f - time_factor);

      r_unfinished_particle_indices.append(pindex);
      r_remaining_durations.append(remaining_duration);
    }
  }
}

BLI_NOINLINE static void run_actions(AttributeArrays attributes,
                                     ArrayRef<SmallVector<uint>> particles_per_event,
                                     ArrayRef<Event *> events,
                                     ArrayRef<Action *> action_per_event)
{
  for (uint event_index = 0; event_index < events.size(); event_index++) {
    Action *action = action_per_event[event_index];
    ParticleSet particles(attributes, particles_per_event[event_index]);
    action->execute(particles);
  }
}

/* Evaluate Forces
 ***********************************************/

BLI_NOINLINE static void compute_combined_forces_on_particles(ParticleSet particles,
                                                              ArrayRef<Force *> forces,
                                                              ArrayRef<float3> r_force_vectors)
{
  BLI_assert(particles.size() == r_force_vectors.size());
  r_force_vectors.fill({0, 0, 0});
  for (Force *force : forces) {
    force->add_force(particles, r_force_vectors);
  }
}

/* Step individual particles.
 **********************************************/

BLI_NOINLINE static void compute_ideal_attribute_offsets(ParticleSet particles,
                                                         ArrayRef<float> durations,
                                                         ParticleInfluences &influences,
                                                         IdealOffsets r_offsets)
{
  BLI_assert(particles.size() == durations.size());
  BLI_assert(particles.size() == r_offsets.position_offsets.size());
  BLI_assert(particles.size() == r_offsets.velocity_offsets.size());

  SmallVector<float3> combined_force{particles.size()};
  compute_combined_forces_on_particles(particles, influences.forces(), combined_force);

  auto velocities = particles.attributes().get_float3("Velocity");

  for (uint i : particles.range()) {
    uint pindex = particles.pindex_of(i);

    float mass = 1.0f;
    float duration = durations[i];

    r_offsets.velocity_offsets[i] = duration * combined_force[i] / mass;
    r_offsets.position_offsets[i] = duration *
                                    (velocities[pindex] + r_offsets.velocity_offsets[i] * 0.5f);
  }
}

BLI_NOINLINE static void simulate_to_next_event(ParticleSet particles,
                                                ArrayRef<float> durations,
                                                float end_time,
                                                ParticleInfluences &influences,
                                                ArrayRef<float> last_event_times,
                                                SmallVector<uint> &r_unfinished_particle_indices,
                                                SmallVector<float> &r_remaining_durations)
{
  SmallVector<float3> position_offsets(particles.size());
  SmallVector<float3> velocity_offsets(particles.size());
  IdealOffsets ideal_offsets{position_offsets, velocity_offsets};

  compute_ideal_attribute_offsets(particles, durations, influences, ideal_offsets);

  SmallVector<int> next_event_indices(particles.size());
  SmallVector<float> time_factors_to_next_event(particles.size());

  find_next_event_per_particle(particles,
                               ideal_offsets,
                               durations,
                               end_time,
                               influences.events(),
                               last_event_times,
                               next_event_indices,
                               time_factors_to_next_event);

  forward_particles_to_next_event(particles, ideal_offsets, time_factors_to_next_event);

  SmallVector<SmallVector<uint>> particles_per_event(influences.events().size());
  find_particles_per_event(particles.indices(), next_event_indices, particles_per_event);
  run_actions(particles.attributes(),
              particles_per_event,
              influences.events(),
              influences.action_per_event());

  find_unfinished_particles(particles.indices(),
                            next_event_indices,
                            time_factors_to_next_event,
                            durations,
                            particles.attributes().get_byte("Kill State"),
                            r_unfinished_particle_indices,
                            r_remaining_durations);
}

BLI_NOINLINE static void simulate_with_max_n_events(
    uint max_events,
    ParticleSet particles,
    ArrayRef<float> durations,
    float end_time,
    ParticleInfluences &influences,
    SmallVector<uint> &r_unfinished_particle_indices,
    SmallVector<float> &r_remaining_durations)
{
  SmallVector<float> last_event_times;
  ArrayRef<uint> remaining_particle_indices = particles.indices();

  for (uint iteration = 0; iteration < max_events; iteration++) {
    r_unfinished_particle_indices.clear();
    r_remaining_durations.clear();

    ParticleSet particles_to_simulate(particles.attributes(), remaining_particle_indices);
    simulate_to_next_event(particles_to_simulate,
                           durations,
                           end_time,
                           influences,
                           last_event_times,
                           r_unfinished_particle_indices,
                           r_remaining_durations);
    BLI_assert(r_unfinished_particle_indices.size() == r_remaining_durations.size());

    if (r_unfinished_particle_indices.size() == 0) {
      break;
    }

    remaining_particle_indices = r_unfinished_particle_indices;
    durations = r_remaining_durations;

    last_event_times.clear();
    for (float duration : durations) {
      last_event_times.append(end_time - duration);
    }
  }
}

BLI_NOINLINE static void simulate_ignoring_events(ParticleSet particles,
                                                  ArrayRef<float> durations,
                                                  ParticleInfluences &influences)
{
  SmallVector<float3> position_offsets{particles.size()};
  SmallVector<float3> velocity_offsets{particles.size()};
  IdealOffsets offsets{position_offsets, velocity_offsets};

  compute_ideal_attribute_offsets(particles, durations, influences, offsets);

  auto positions = particles.attributes().get_float3("Position");
  auto velocities = particles.attributes().get_float3("Velocity");

  for (uint i : particles.indices()) {
    uint pindex = particles.pindex_of(i);

    positions[pindex] += offsets.position_offsets[i];
    velocities[pindex] += offsets.velocity_offsets[i];
  }
}

BLI_NOINLINE static void step_individual_particles(ParticleSet particles,
                                                   ArrayRef<float> durations,
                                                   float end_time,
                                                   ParticleInfluences &influences)
{
  SmallVector<uint> unfinished_particle_indices;
  SmallVector<float> remaining_durations;

  simulate_with_max_n_events(10,
                             particles,
                             durations,
                             end_time,
                             influences,
                             unfinished_particle_indices,
                             remaining_durations);

  ParticleSet remaining_particles(particles.attributes(), unfinished_particle_indices);
  simulate_ignoring_events(remaining_particles, remaining_durations, influences);
}

struct StepBlocksParallelData {
  ArrayRef<ParticlesBlock *> blocks;
  ArrayRef<float> all_durations;
  float end_time;
  ParticleInfluences &influences;
};

BLI_NOINLINE static void step_individual_particles_cb(
    void *__restrict userdata, const int index, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  StepBlocksParallelData *data = (StepBlocksParallelData *)userdata;
  ParticlesBlock *block = data->blocks[index];

  uint active_amount = block->active_amount();
  ParticleSet active_particles(block->slice_active(),
                               static_number_range_ref().take_front(active_amount));
  step_individual_particles(active_particles,
                            data->all_durations.take_front(active_amount),
                            data->end_time,
                            data->influences);
}

BLI_NOINLINE static void step_individual_particles(ArrayRef<ParticlesBlock *> blocks,
                                                   TimeSpan time_span,
                                                   ParticleInfluences &influences)
{
  if (blocks.size() == 0) {
    return;
  }

  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  uint block_size = blocks[0]->container().block_size();
  SmallVector<float> all_durations(block_size);
  all_durations.fill(time_span.duration());

  StepBlocksParallelData data = {blocks, all_durations, time_span.end(), influences};

  BLI_task_parallel_range(
      0, blocks.size(), (void *)&data, step_individual_particles_cb, &settings);
}

/* Delete particles.
 **********************************************/

BLI_NOINLINE static void delete_tagged_particles_and_reorder(ParticlesBlock &block)
{
  auto kill_states = block.slice_active().get_byte("Kill State");

  uint index = 0;
  while (index < block.active_amount()) {
    if (kill_states[index] == 1) {
      block.move(block.active_amount() - 1, index);
      block.active_amount() -= 1;
    }
    else {
      index++;
    }
  }
}

BLI_NOINLINE static void delete_tagged_particles(ArrayRef<ParticlesBlock *> blocks)
{
  for (ParticlesBlock *block : blocks) {
    delete_tagged_particles_and_reorder(*block);
  }
}

/* Emit new particles from emitters.
 **********************************************/

BLI_NOINLINE static void emit_new_particles_from_emitter(ParticlesContainer &container,
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
    ParticleSet emitted_particles(emitted_attributes,
                                  static_number_range_ref().take_front(emitted_attributes.size()));
    step_individual_particles(
        emitted_particles, initial_step_durations, time_span.end(), influences);
  }
}

BLI_NOINLINE static void emit_new_particles_from_emitters(ParticlesContainer &container,
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

BLI_NOINLINE static void compress_all_blocks(ParticlesContainer &particles)
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

  ParticlesContainer &particles = *state.particle_containers().lookup(0);
  ParticleType &type = description.particle_type(0);

  step_individual_particles(
      particles.active_blocks().to_small_vector(), time_span, type.influences());
  emit_new_particles_from_emitters(particles, type.emitters(), type.influences(), time_span);
  delete_tagged_particles(particles.active_blocks().to_small_vector());
  compress_all_blocks(particles);
}

}  // namespace BParticles
