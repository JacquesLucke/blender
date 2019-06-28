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
                                                      ArrayRef<float> r_time_factors_to_next_event)
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
}

BLI_NOINLINE static void forward_particles_to_next_event(
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

BLI_NOINLINE static void run_actions(BlockAllocator &block_allocator,
                                     ParticlesBlock &block,
                                     ArrayRef<SmallVector<uint>> particles_per_event,
                                     ArrayRef<Event *> events,
                                     ArrayRef<Action *> action_per_event)
{
  for (uint event_index = 0; event_index < events.size(); event_index++) {
    Action *action = action_per_event[event_index];
    ParticleSet particles(block, particles_per_event[event_index]);

    ActionInterface interface(particles, block_allocator);
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
                                                ArrayRef<float> durations,
                                                float end_time,
                                                ParticleType &particle_type,
                                                ArrayRef<float> last_event_times,
                                                SmallVector<uint> &r_unfinished_particle_indices,
                                                SmallVector<float> &r_remaining_durations)
{
  SmallVector<float3> position_offsets(particles.size());
  SmallVector<float3> velocity_offsets(particles.size());
  IdealOffsets ideal_offsets{position_offsets, velocity_offsets};

  compute_ideal_attribute_offsets(particles, durations, particle_type, ideal_offsets);

  SmallVector<int> next_event_indices(particles.size());
  SmallVector<float> time_factors_to_next_event(particles.size());

  find_next_event_per_particle(particles,
                               ideal_offsets,
                               durations,
                               end_time,
                               particle_type.events(),
                               last_event_times,
                               next_event_indices,
                               time_factors_to_next_event);

  forward_particles_to_next_event(particles, ideal_offsets, time_factors_to_next_event);

  SmallVector<SmallVector<uint>> particles_per_event(particle_type.events().size());
  find_particles_per_event(particles.indices(), next_event_indices, particles_per_event);
  run_actions(block_allocator,
              particles.block(),
              particles_per_event,
              particle_type.events(),
              particle_type.action_per_event());

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
    BlockAllocator &block_allocator,
    ParticleSet particles,
    ArrayRef<float> durations,
    float end_time,
    ParticleType &particle_type,
    SmallVector<uint> &r_unfinished_particle_indices,
    SmallVector<float> &r_remaining_durations)
{
  SmallVector<float> last_event_times;
  ArrayRef<uint> remaining_particle_indices = particles.indices();

  for (uint iteration = 0; iteration < max_events; iteration++) {
    r_unfinished_particle_indices.clear();
    r_remaining_durations.clear();

    ParticleSet particles_to_simulate(particles.block(), remaining_particle_indices);
    simulate_to_next_event(block_allocator,
                           particles_to_simulate,
                           durations,
                           end_time,
                           particle_type,
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
                                                  ParticleType &particle_type)
{
  SmallVector<float3> position_offsets{particles.size()};
  SmallVector<float3> velocity_offsets{particles.size()};
  IdealOffsets offsets{position_offsets, velocity_offsets};

  compute_ideal_attribute_offsets(particles, durations, particle_type, offsets);

  auto positions = particles.attributes().get_float3("Position");
  auto velocities = particles.attributes().get_float3("Velocity");

  for (uint i : particles.indices()) {
    uint pindex = particles.get_particle_index(i);

    positions[pindex] += offsets.position_offsets[i];
    velocities[pindex] += offsets.velocity_offsets[i];
  }
}

BLI_NOINLINE static void step_particle_set(BlockAllocator &block_allocator,
                                           ParticleSet particles,
                                           ArrayRef<float> durations,
                                           float end_time,
                                           ParticleType &particle_type)
{
  SmallVector<uint> unfinished_particle_indices;
  SmallVector<float> remaining_durations;

  simulate_with_max_n_events(10,
                             block_allocator,
                             particles,
                             durations,
                             end_time,
                             particle_type,
                             unfinished_particle_indices,
                             remaining_durations);

  ParticleSet remaining_particles(particles.block(), unfinished_particle_indices);
  simulate_ignoring_events(remaining_particles, remaining_durations, particle_type);
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
  ParticlesBlock &block = *data->blocks[index];

  BlockAllocator block_allocator = data->block_allocators.get_threadlocal_allocator(
      tls->thread_id);

  ParticlesState &state = block_allocator.particles_state();
  uint particle_type_id = state.particle_container_id(block.container());

  ParticleType &particle_type = data->step_description.particle_type(particle_type_id);

  uint active_amount = block.active_amount();
  ParticleSet active_particles(block, static_number_range_ref(0, active_amount));
  step_particle_set(block_allocator,
                    active_particles,
                    data->all_durations.take_front(active_amount),
                    data->end_time,
                    particle_type);
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
  ParticlesBlock &block = *data->blocks[index];

  BlockAllocator block_allocator = data->block_allocators.get_threadlocal_allocator(
      tls->thread_id);

  ParticlesState &state = block_allocator.particles_state();
  uint particle_type_id = state.particle_container_id(block.container());

  ParticleType &particle_type = data->step_description.particle_type(particle_type_id);

  uint active_amount = block.active_amount();
  SmallVector<float> durations(active_amount);

  auto birth_times = block.slice_active().get_float("Birth Time");
  for (uint i = 0; i < active_amount; i++) {
    durations[i] = data->end_time - birth_times[i];
  }

  ParticleSet active_particles(block, static_number_range_ref(0, active_amount));
  step_particle_set(block_allocator, active_particles, durations, data->end_time, particle_type);
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

/* Emit new particles from emitters.
 **********************************************/

BLI_NOINLINE static void emit_new_particles_from_emitter(BlockAllocator &block_allocator,
                                                         TimeSpan time_span,
                                                         Emitter &emitter)
{
  EmitterInterface interface(block_allocator);
  emitter.emit(interface);

  for (TimeSpanEmitTarget *target_ptr : interface.targets()) {
    TimeSpanEmitTarget &target = *target_ptr;

    ArrayRef<float> all_birth_moments = target.birth_moments();
    uint particle_count = 0;

    for (uint part = 0; part < target.part_amount(); part++) {
      ParticlesBlock &block = *target.blocks()[part];
      Range<uint> range = target.ranges()[part];
      AttributeArrays attributes = block.slice(range);

      ArrayRef<float> birth_moments = all_birth_moments.slice(particle_count, range.size());

      auto birth_times = attributes.get_float("Birth Time");
      for (uint i = 0; i < birth_moments.size(); i++) {
        birth_times[i] = time_span.interpolate(birth_moments[i]);
      }

      particle_count += range.size();
    }
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
    emit_new_particles_from_emitter(emitter_allocator, time_span, *emitter);
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
