#include "simulate.hpp"
#include "time_span.hpp"

#include "BLI_lazy_init.hpp"
#include "BLI_task.h"

#define USE_THREADING true

namespace BParticles {

/* Static Data
 **************************************************/

BLI_LAZY_INIT_STATIC(SmallVector<uint>, static_number_range_vector)
{
  return Range<uint>(0, 10000).to_small_vector();
}

static ArrayRef<uint> static_number_range_ref(uint start, uint length)
{
  return ArrayRef<uint>(static_number_range_vector()).slice(start, length);
}

static ArrayRef<uint> static_number_range_ref(Range<uint> range)
{
  if (range.size() == 0) {
    return {};
  }
  return static_number_range_ref(range.first(), range.size());
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
                                                      ArrayRef<float> r_time_factors_to_next_event,
                                                      SmallVector<uint> &r_indices_with_event)
{
  r_next_event_indices.fill(-1);
  r_time_factors_to_next_event.fill(1.0f);

  for (uint event_index = 0; event_index < events.size(); event_index++) {
    SmallVector<uint> triggered_indices;
    SmallVector<float> triggered_time_factors;

    Event *event = events[event_index];
    EventInterface interface(
        particles, ideal_offsets, durations, end_time, triggered_indices, triggered_time_factors);
    event->filter(interface);

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

  for (uint i = 0; i < r_next_event_indices.size(); i++) {
    if (r_next_event_indices[i] != -1) {
      r_indices_with_event.append(i);
    }
  }
}

BLI_NOINLINE static void forward_particles_to_next_event_or_end(
    ParticleSet particles, IdealOffsets &ideal_offsets, ArrayRef<float> time_factors_to_next_event)
{
  auto positions = particles.attributes().get_float3("Position");
  auto velocities = particles.attributes().get_float3("Velocity");

  for (uint i : particles.range()) {
    uint pindex = particles.get_particle_index(i);
    float time_factor = time_factors_to_next_event[i];
    positions[pindex] += time_factor * ideal_offsets.position_offsets[i];
    velocities[pindex] += time_factor * ideal_offsets.velocity_offsets[i];
  }
}

BLI_NOINLINE static void update_ideal_offsets_for_particles_with_events(
    ArrayRef<uint> indices_with_events,
    ArrayRef<float> time_factors_to_next_event,
    IdealOffsets &ideal_offsets)
{
  for (uint i : indices_with_events) {
    float factor = 1.0f - time_factors_to_next_event[i];
    ideal_offsets.position_offsets[i] *= factor;
    ideal_offsets.velocity_offsets[i] *= factor;
  }
}

BLI_NOINLINE static void find_particles_per_event(
    ArrayRef<uint> indices_with_events,
    ArrayRef<uint> particle_indices,
    ArrayRef<int> next_event_indices,
    ArrayRef<SmallVector<uint>> r_particles_per_event)
{
  for (uint i : indices_with_events) {
    int event_index = next_event_indices[i];
    BLI_assert(event_index >= 0);
    uint pindex = particle_indices[i];
    r_particles_per_event[event_index].append(pindex);
  }
}

BLI_NOINLINE static void compute_current_time_per_particle(
    ArrayRef<uint> indices_with_events,
    ArrayRef<float> durations,
    float end_time,
    ArrayRef<int> next_event_indices,
    ArrayRef<float> time_factors_to_next_event,
    ArrayRef<SmallVector<float>> r_current_time_per_particle)
{
  for (uint i : indices_with_events) {
    int event_index = next_event_indices[i];
    BLI_assert(event_index >= 0);
    r_current_time_per_particle[event_index].append(
        end_time - durations[i] * (1.0f - time_factors_to_next_event[i]));
  }
}

BLI_NOINLINE static void find_unfinished_particles(ArrayRef<uint> indices_with_event,
                                                   ArrayRef<uint> particle_indices,
                                                   ArrayRef<float> time_factors_to_next_event,
                                                   ArrayRef<float> durations,
                                                   ArrayRef<uint8_t> kill_states,
                                                   SmallVector<uint> &r_unfinished_indices,
                                                   SmallVector<float> &r_remaining_durations)
{

  for (uint i : indices_with_event) {
    uint pindex = particle_indices[i];
    if (kill_states[pindex] == 0) {
      float time_factor = time_factors_to_next_event[i];
      float remaining_duration = durations[i] * (1.0f - time_factor);

      r_unfinished_indices.append(i);
      r_remaining_durations.append(remaining_duration);
    }
  }
}

BLI_NOINLINE static void run_actions(BlockAllocator &block_allocator,
                                     ParticlesBlock &block,
                                     ArrayRef<SmallVector<uint>> particles_per_event,
                                     ArrayRef<SmallVector<float>> current_time_per_particle,
                                     ArrayRef<Event *> events,
                                     ArrayRef<Action *> action_per_event)
{
  for (uint event_index = 0; event_index < events.size(); event_index++) {
    Action *action = action_per_event[event_index];
    ParticleSet particles(block, particles_per_event[event_index]);
    if (particles.size() == 0) {
      continue;
    }

    ActionInterface interface(particles, block_allocator, current_time_per_particle[event_index]);
    action->execute(interface);
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
                                                         ParticleType &particle_type,
                                                         IdealOffsets r_offsets)
{
  BLI_assert(particles.size() == durations.size());
  BLI_assert(particles.size() == r_offsets.position_offsets.size());
  BLI_assert(particles.size() == r_offsets.velocity_offsets.size());

  SmallVector<float3> combined_force{particles.size()};
  compute_combined_forces_on_particles(particles, particle_type.forces(), combined_force);

  auto velocities = particles.attributes().get_float3("Velocity");

  for (uint i : particles.range()) {
    uint pindex = particles.get_particle_index(i);

    float mass = 1.0f;
    float duration = durations[i];

    r_offsets.velocity_offsets[i] = duration * combined_force[i] / mass;
    r_offsets.position_offsets[i] = duration *
                                    (velocities[pindex] + r_offsets.velocity_offsets[i] * 0.5f);
  }
}

BLI_NOINLINE static void simulate_to_next_event(BlockAllocator &block_allocator,
                                                ParticleSet particles,
                                                IdealOffsets ideal_offsets,
                                                ArrayRef<float> durations,
                                                float end_time,
                                                ParticleType &particle_type,
                                                ArrayRef<float> last_event_times,
                                                SmallVector<uint> &r_unfinished_indices,
                                                SmallVector<float> &r_remaining_durations)
{
  SmallVector<int> next_event_indices(particles.size());
  SmallVector<float> time_factors_to_next_event(particles.size());
  SmallVector<uint> indices_with_event;

  find_next_event_per_particle(particles,
                               ideal_offsets,
                               durations,
                               end_time,
                               particle_type.events(),
                               last_event_times,
                               next_event_indices,
                               time_factors_to_next_event,
                               indices_with_event);

  forward_particles_to_next_event_or_end(particles, ideal_offsets, time_factors_to_next_event);
  update_ideal_offsets_for_particles_with_events(
      indices_with_event, time_factors_to_next_event, ideal_offsets);

  SmallVector<SmallVector<uint>> particles_per_event(particle_type.events().size());
  find_particles_per_event(
      indices_with_event, particles.indices(), next_event_indices, particles_per_event);

  SmallVector<SmallVector<float>> current_time_per_particle(particle_type.events().size());
  compute_current_time_per_particle(indices_with_event,
                                    durations,
                                    end_time,
                                    next_event_indices,
                                    time_factors_to_next_event,
                                    current_time_per_particle);

  run_actions(block_allocator,
              particles.block(),
              particles_per_event,
              current_time_per_particle,
              particle_type.events(),
              particle_type.action_per_event());

  find_unfinished_particles(indices_with_event,
                            particles.indices(),
                            time_factors_to_next_event,
                            durations,
                            particles.attributes().get_byte("Kill State"),
                            r_unfinished_indices,
                            r_remaining_durations);
}

BLI_NOINLINE static void simulate_with_max_n_events(uint UNUSED(max_events),
                                                    BlockAllocator &block_allocator,
                                                    ParticleSet particles,
                                                    IdealOffsets ideal_offsets,
                                                    ArrayRef<float> durations,
                                                    float end_time,
                                                    ParticleType &particle_type,
                                                    SmallVector<uint> &r_unfinished_indices)
{
  SmallVector<float> last_event_times;
  SmallVector<float> remaining_durations;

  simulate_to_next_event(block_allocator,
                         particles,
                         ideal_offsets,
                         durations,
                         end_time,
                         particle_type,
                         last_event_times,
                         r_unfinished_indices,
                         remaining_durations);
}

BLI_NOINLINE static void apply_remaining_offsets(ParticleSet particles, IdealOffsets ideal_offsets)
{
  auto positions = particles.attributes().get_float3("Position");
  auto velocities = particles.attributes().get_float3("Velocity");

  for (uint i : particles.indices()) {
    uint pindex = particles.get_particle_index(i);

    positions[pindex] += ideal_offsets.position_offsets[i];
    velocities[pindex] += ideal_offsets.velocity_offsets[i];
  }
}

BLI_NOINLINE static void step_particle_set(BlockAllocator &block_allocator,
                                           ParticleSet particles,
                                           ArrayRef<float> durations,
                                           float end_time,
                                           ParticleType &particle_type)
{
  SmallVector<float3> position_offsets(particles.size());
  SmallVector<float3> velocity_offsets(particles.size());
  IdealOffsets ideal_offsets{position_offsets, velocity_offsets};
  compute_ideal_attribute_offsets(particles, durations, particle_type, ideal_offsets);

  SmallVector<uint> unfinished_indices;
  simulate_with_max_n_events(10,
                             block_allocator,
                             particles,
                             ideal_offsets,
                             durations,
                             end_time,
                             particle_type,
                             unfinished_indices);

  SmallVector<uint> remaining_particle_indices(unfinished_indices.size());
  SmallVector<float3> remaining_position_offsets(unfinished_indices.size());
  SmallVector<float3> remaining_velocity_offsets(unfinished_indices.size());
  for (uint i = 0; i < unfinished_indices.size(); i++) {
    uint index = unfinished_indices[i];
    uint pindex = particles.get_particle_index(index);
    remaining_position_offsets[i] = ideal_offsets.position_offsets[index];
    remaining_velocity_offsets[i] = ideal_offsets.velocity_offsets[index];
    remaining_particle_indices[i] = pindex;
  }

  ParticleSet remaining_particles(particles.block(), remaining_particle_indices);
  apply_remaining_offsets(remaining_particles, ideal_offsets);
}

BLI_NOINLINE static void simulate_block(BlockAllocator &block_allocator,
                                        ParticlesBlock &block,
                                        ParticleType &particle_type,
                                        ArrayRef<float> durations,
                                        float end_time)
{
  step_particle_set(block_allocator,
                    ParticleSet(block, static_number_range_ref(0, block.active_amount())),
                    durations,
                    end_time,
                    particle_type);
}

class BlockAllocators {
 private:
  ParticlesState &m_state;
  SmallVector<BlockAllocator *> m_allocators;
  SmallMap<int, BlockAllocator *> m_allocator_per_thread_id;
  std::mutex m_access_mutex;

 public:
  BlockAllocators(ParticlesState &state) : m_state(state)
  {
  }

  ~BlockAllocators()
  {
    for (BlockAllocator *allocator : m_allocators) {
      delete allocator;
    }
  }

  BlockAllocator &get_standalone_allocator()
  {
    std::lock_guard<std::mutex> lock(m_access_mutex);

    BlockAllocator *new_allocator = new BlockAllocator(m_state);
    m_allocators.append(new_allocator);
    return *new_allocator;
  }

  BlockAllocator &get_threadlocal_allocator(int thread_id)
  {
    std::lock_guard<std::mutex> lock(m_access_mutex);

    if (!m_allocator_per_thread_id.contains(thread_id)) {
      BlockAllocator *new_allocator = new BlockAllocator(m_state);
      m_allocators.append(new_allocator);
      m_allocator_per_thread_id.add_new(thread_id, new_allocator);
    }
    return *m_allocator_per_thread_id.lookup(thread_id);
  }

  ArrayRef<BlockAllocator *> allocators()
  {
    return m_allocators;
  }

  SmallVector<ParticlesBlock *> all_allocated_blocks()
  {
    SmallVector<ParticlesBlock *> blocks;
    for (BlockAllocator *allocator : m_allocators) {
      blocks.extend(allocator->allocated_blocks());
    }
    return blocks;
  }
};

struct SimulateTimeSpanData {
  ArrayRef<ParticlesBlock *> blocks;
  ArrayRef<float> all_durations;
  float end_time;
  BlockAllocators &block_allocators;
  StepDescription &step_description;
};

BLI_NOINLINE static void simulate_block_time_span_cb(void *__restrict userdata,
                                                     const int index,
                                                     const ParallelRangeTLS *__restrict tls)
{
  SimulateTimeSpanData *data = (SimulateTimeSpanData *)userdata;

  BlockAllocator &block_allocator = data->block_allocators.get_threadlocal_allocator(
      tls->thread_id);

  ParticlesBlock &block = *data->blocks[index];
  ParticlesState &state = block_allocator.particles_state();
  uint particle_type_id = state.particle_container_id(block.container());
  ParticleType &particle_type = data->step_description.particle_type(particle_type_id);

  simulate_block(block_allocator,
                 block,
                 particle_type,
                 data->all_durations.take_back(block.active_amount()),
                 data->end_time);
}

BLI_NOINLINE static void simulate_blocks_for_time_span(BlockAllocators &block_allocators,
                                                       ArrayRef<ParticlesBlock *> blocks,
                                                       StepDescription &step_description,
                                                       TimeSpan time_span)
{
  if (blocks.size() == 0) {
    return;
  }

  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = USE_THREADING;

  uint block_size = blocks[0]->container().block_size();
  SmallVector<float> all_durations(block_size);
  all_durations.fill(time_span.duration());

  SimulateTimeSpanData data = {
      blocks, all_durations, time_span.end(), block_allocators, step_description};

  BLI_task_parallel_range(0, blocks.size(), (void *)&data, simulate_block_time_span_cb, &settings);
}

struct SimulateFromBirthData {
  ArrayRef<ParticlesBlock *> blocks;
  float end_time;
  BlockAllocators &block_allocators;
  StepDescription &step_description;
};

BLI_NOINLINE static void simulate_block_from_birth_cb(void *__restrict userdata,
                                                      const int index,
                                                      const ParallelRangeTLS *__restrict tls)
{
  SimulateFromBirthData *data = (SimulateFromBirthData *)userdata;

  BlockAllocator &block_allocator = data->block_allocators.get_threadlocal_allocator(
      tls->thread_id);

  ParticlesBlock &block = *data->blocks[index];
  ParticlesState &state = block_allocator.particles_state();

  uint particle_type_id = state.particle_container_id(block.container());
  ParticleType &particle_type = data->step_description.particle_type(particle_type_id);

  uint active_amount = block.active_amount();
  SmallVector<float> durations(active_amount);
  auto birth_times = block.slice_active().get_float("Birth Time");
  for (uint i = 0; i < active_amount; i++) {
    durations[i] = data->end_time - birth_times[i];
  }
  simulate_block(block_allocator, block, particle_type, durations, data->end_time);
}

BLI_NOINLINE static void simulate_blocks_from_birth_to_current_time(
    BlockAllocators &block_allocators,
    ArrayRef<ParticlesBlock *> blocks,
    StepDescription &step_description,
    float end_time)
{
  if (blocks.size() == 0) {
    return;
  }

  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = USE_THREADING;

  SimulateFromBirthData data = {blocks, end_time, block_allocators, step_description};
  BLI_task_parallel_range(
      0, blocks.size(), (void *)&data, simulate_block_from_birth_cb, &settings);
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

/* Compress particle blocks.
 **************************************************/

BLI_NOINLINE static void compress_all_blocks(ParticlesContainer &particles)
{
  SmallVector<ParticlesBlock *> blocks = particles.active_blocks().to_small_vector();
  ParticlesBlock::Compress(blocks);

  for (ParticlesBlock *block : blocks) {
    if (block->is_empty()) {
      particles.release_block(*block);
    }
  }
}

/* Fix state based on description.
 *****************************************************/

BLI_NOINLINE static void ensure_required_containers_exist(
    SmallMap<uint, ParticlesContainer *> &containers, StepDescription &description)
{
  for (uint type_id : description.particle_type_ids()) {
    if (!containers.contains(type_id)) {
      ParticlesContainer *container = new ParticlesContainer({}, 1000);
      containers.add_new(type_id, container);
    }
  }
}

BLI_NOINLINE static AttributesInfo build_attribute_info_for_type(ParticleType &UNUSED(type),
                                                                 AttributesInfo &UNUSED(last_info))
{
  AttributesInfo new_info{{"Kill State"}, {"Birth Time"}, {"Position", "Velocity"}};
  return new_info;
}

BLI_NOINLINE static void ensure_required_attributes_exist(
    SmallMap<uint, ParticlesContainer *> &containers, StepDescription &description)
{
  for (uint type_id : description.particle_type_ids()) {
    ParticleType &type = description.particle_type(type_id);
    ParticlesContainer &container = *containers.lookup(type_id);

    AttributesInfo new_attributes_info = build_attribute_info_for_type(
        type, container.attributes_info());
    container.update_attributes(new_attributes_info);
  }
}

/* Main Entry Point
 **************************************************/

void simulate_step(ParticlesState &state, StepDescription &description)
{
  TimeSpan time_span(state.m_current_time, description.step_duration());
  state.m_current_time = time_span.end();

  auto &containers = state.particle_containers();
  ensure_required_containers_exist(containers, description);
  ensure_required_attributes_exist(containers, description);

  BlockAllocators block_allocators(state);

  SmallVector<ParticlesBlock *> blocks_to_simulate_next;
  for (uint type_id : description.particle_type_ids()) {
    ParticlesContainer &container = *containers.lookup(type_id);
    blocks_to_simulate_next.extend(container.active_blocks().to_small_vector());
  }
  simulate_blocks_for_time_span(block_allocators, blocks_to_simulate_next, description, time_span);

  BlockAllocator &emitter_allocator = block_allocators.get_standalone_allocator();
  for (Emitter *emitter : description.emitters()) {
    EmitterInterface interface(emitter_allocator, time_span);
    emitter->emit(interface);
  }

  blocks_to_simulate_next = block_allocators.all_allocated_blocks();
  while (blocks_to_simulate_next.size() > 0) {
    BlockAllocators allocators(state);
    simulate_blocks_from_birth_to_current_time(
        allocators, blocks_to_simulate_next, description, time_span.end());
    blocks_to_simulate_next = allocators.all_allocated_blocks();
  }

  for (uint type_id : description.particle_type_ids()) {
    ParticlesContainer &container = *containers.lookup(type_id);
    delete_tagged_particles(container.active_blocks().to_small_vector());
  }

  for (uint type_id : description.particle_type_ids()) {
    ParticlesContainer &container = *containers.lookup(type_id);
    compress_all_blocks(container);
  }
}

}  // namespace BParticles
