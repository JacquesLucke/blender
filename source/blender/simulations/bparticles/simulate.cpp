#include "simulate.hpp"
#include "time_span.hpp"

#include "BLI_lazy_init.hpp"
#include "BLI_task.h"
#include "BLI_timeit.hpp"

#define USE_THREADING false

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
                                                      AttributeArrays &attribute_offsets,
                                                      ArrayRef<float> durations,
                                                      float end_time,
                                                      ArrayRef<EventAction *> event_actions,
                                                      ArrayRef<float> last_event_times,
                                                      ArrayRef<int> r_next_event_indices,
                                                      ArrayRef<float> r_time_factors_to_next_event,
                                                      SmallVector<uint> &r_indices_with_event)
{
  r_next_event_indices.fill(-1);
  r_time_factors_to_next_event.fill(1.0f);

  for (uint event_index = 0; event_index < event_actions.size(); event_index++) {
    SmallVector<uint> triggered_indices;
    SmallVector<float> triggered_time_factors;

    EventAction *event_action = event_actions[event_index];
    EventInterface interface(particles,
                             attribute_offsets,
                             durations,
                             end_time,
                             triggered_indices,
                             triggered_time_factors);
    event_action->filter(interface);

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
    ParticleSet particles,
    AttributeArrays attribute_offsets,
    ArrayRef<float> time_factors_to_next_event)
{
  for (uint attribute_index : attribute_offsets.info().float3_attributes()) {
    StringRef name = attribute_offsets.info().name_of(attribute_index);

    auto values = particles.attributes().get_float3(name);
    auto offsets = attribute_offsets.get_float3(attribute_index);

    if (particles.indices_are_trivial()) {
      for (uint pindex = 0; pindex < particles.size(); pindex++) {
        float time_factor = time_factors_to_next_event[pindex];
        values[pindex] += time_factor * offsets[pindex];
      }
    }
    else {
      for (uint i : particles.range()) {
        uint pindex = particles.get_particle_index(i);
        float time_factor = time_factors_to_next_event[i];
        values[pindex] += time_factor * offsets[pindex];
      }
    }
  }
}

BLI_NOINLINE static void update_remaining_attribute_offsets(
    ParticleSet particles_with_events,
    ArrayRef<float> time_factors_to_next_event,
    AttributeArrays attribute_offsets)
{
  for (uint attribute_index : attribute_offsets.info().float3_attributes()) {
    auto offsets = attribute_offsets.get_float3(attribute_index);

    for (uint i : particles_with_events.range()) {
      uint pindex = particles_with_events.get_particle_index(i);
      float factor = 1.0f - time_factors_to_next_event[i];
      offsets[pindex] *= factor;
    }
  }
}

BLI_NOINLINE static void find_particle_indices_per_event(
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

BLI_NOINLINE static void find_unfinished_particles(
    ArrayRef<uint> indices_with_event,
    ArrayRef<uint> particle_indices,
    ArrayRef<float> time_factors_to_next_event,
    ArrayRef<float> durations,
    ArrayRef<uint8_t> kill_states,
    SmallVector<uint> &r_unfinished_particle_indices,
    SmallVector<float> &r_remaining_durations)
{

  for (uint i : indices_with_event) {
    uint pindex = particle_indices[i];
    if (kill_states[pindex] == 0) {
      float time_factor = time_factors_to_next_event[i];
      float remaining_duration = durations[i] * (1.0f - time_factor);

      r_unfinished_particle_indices.append(pindex);
      r_remaining_durations.append(remaining_duration);
    }
  }
}

BLI_NOINLINE static void run_actions(BlockAllocator &block_allocator,
                                     ParticlesBlock &block,
                                     ArrayRef<SmallVector<uint>> particle_indices_per_event,
                                     ArrayRef<SmallVector<float>> current_time_per_particle,
                                     ArrayRef<EventAction *> event_actions)
{
  BLI_assert(event_actions.size() == particle_indices_per_event.size());
  BLI_assert(event_actions.size() == current_time_per_particle.size());

  for (uint event_index = 0; event_index < event_actions.size(); event_index++) {
    EventAction *event_action = event_actions[event_index];
    ParticleSet particles(block, particle_indices_per_event[event_index]);
    if (particles.size() == 0) {
      continue;
    }

    ActionInterface interface(particles, block_allocator, current_time_per_particle[event_index]);
    event_action->execute(interface);
  }
}

/* Step individual particles.
 **********************************************/

BLI_NOINLINE static void simulate_to_next_event(BlockAllocator &block_allocator,
                                                ParticleSet particles,
                                                AttributeArrays attribute_offsets,
                                                ArrayRef<float> durations,
                                                float end_time,
                                                ArrayRef<EventAction *> events,
                                                ArrayRef<float> last_event_times,
                                                SmallVector<uint> &r_unfinished_particle_indices,
                                                SmallVector<float> &r_remaining_durations)
{
  SmallVector<int> next_event_indices(particles.size());
  SmallVector<float> time_factors_to_next_event(particles.size());
  SmallVector<uint> indices_with_event;

  find_next_event_per_particle(particles,
                               attribute_offsets,
                               durations,
                               end_time,
                               events,
                               last_event_times,
                               next_event_indices,
                               time_factors_to_next_event,
                               indices_with_event);

  forward_particles_to_next_event_or_end(particles, attribute_offsets, time_factors_to_next_event);

  SmallVector<uint> particle_indices_with_event(indices_with_event.size());
  for (uint i = 0; i < indices_with_event.size(); i++) {
    particle_indices_with_event[i] = particles.get_particle_index(i);
  }

  ParticleSet particles_with_events(particles.block(), particle_indices_with_event);
  update_remaining_attribute_offsets(
      particles_with_events, time_factors_to_next_event, attribute_offsets);

  SmallVector<SmallVector<uint>> particles_per_event(events.size());
  find_particle_indices_per_event(
      indices_with_event, particles.indices(), next_event_indices, particles_per_event);

  SmallVector<SmallVector<float>> current_time_per_particle(events.size());
  compute_current_time_per_particle(indices_with_event,
                                    durations,
                                    end_time,
                                    next_event_indices,
                                    time_factors_to_next_event,
                                    current_time_per_particle);

  run_actions(
      block_allocator, particles.block(), particles_per_event, current_time_per_particle, events);

  find_unfinished_particles(indices_with_event,
                            particles.indices(),
                            time_factors_to_next_event,
                            durations,
                            particles.attributes().get_byte("Kill State"),
                            r_unfinished_particle_indices,
                            r_remaining_durations);
}

BLI_NOINLINE static void simulate_with_max_n_events(
    uint max_events,
    BlockAllocator &block_allocator,
    ParticlesBlock &block,
    AttributeArrays attribute_offsets,
    ArrayRef<float> durations,
    float end_time,
    ArrayRef<EventAction *> events,
    SmallVector<uint> &r_unfinished_particle_indices)
{
  SmallVector<float> last_event_times;

  /* Handle first event separately to be able to use the static number range. */
  ParticleSet particles_to_simulate(block, static_number_range_ref(block.active_range()));
  SmallVector<uint> unfinished_particle_indices;
  SmallVector<float> remaining_durations;

  simulate_to_next_event(block_allocator,
                         particles_to_simulate,
                         attribute_offsets,
                         durations,
                         end_time,
                         events,
                         last_event_times,
                         unfinished_particle_indices,
                         remaining_durations);

  for (uint iteration = 0; iteration < max_events - 1; iteration++) {
    particles_to_simulate = ParticleSet(block, unfinished_particle_indices);
    SmallVector<uint> unfinished_particle_indices_after;
    SmallVector<float> remaining_durations_after;

    simulate_to_next_event(block_allocator,
                           particles_to_simulate,
                           attribute_offsets,
                           remaining_durations,
                           end_time,
                           events,
                           last_event_times,
                           unfinished_particle_indices_after,
                           remaining_durations_after);

    unfinished_particle_indices = std::move(unfinished_particle_indices_after);
    remaining_durations = std::move(remaining_durations_after);
  }

  r_unfinished_particle_indices = std::move(unfinished_particle_indices);
}

BLI_NOINLINE static void apply_remaining_offsets(ParticleSet particles,
                                                 AttributeArrays attribute_offsets)
{
  for (uint attribute_index : attribute_offsets.info().float3_attributes()) {
    StringRef name = attribute_offsets.info().name_of(attribute_index);

    auto values = particles.attributes().get_float3(name);
    auto offsets = attribute_offsets.get_float3(attribute_index);

    for (uint pindex : particles.indices()) {
      values[pindex] += offsets[pindex];
    }
  }
}

BLI_NOINLINE static void simulate_block(BlockAllocator &block_allocator,
                                        ParticlesBlock &block,
                                        ParticleType &particle_type,
                                        ArrayRef<float> durations,
                                        float end_time)
{
  uint amount = block.active_amount();
  BLI_assert(amount == durations.size());

  Integrator &integrator = particle_type.integrator();
  AttributesInfo &offsets_info = integrator.offset_attributes_info();
  AttributeArraysCore attribute_offsets_core = AttributeArraysCore::NewWithSeparateAllocations(
      offsets_info, amount);
  AttributeArrays attribute_offsets = attribute_offsets_core.slice_all();

  integrator.integrate(block, durations, attribute_offsets);

  ArrayRef<EventAction *> events = particle_type.event_actions();

  if (events.size() == 0) {
    ParticleSet all_particles_in_block(block, static_number_range_ref(block.active_range()));
    apply_remaining_offsets(all_particles_in_block, attribute_offsets);
  }
  else {
    SmallVector<uint> unfinished_particle_indices;
    simulate_with_max_n_events(10,
                               block_allocator,
                               block,
                               attribute_offsets,
                               durations,
                               end_time,
                               events,
                               unfinished_particle_indices);

    ParticleSet remaining_particles(block, unfinished_particle_indices);
    apply_remaining_offsets(remaining_particles, attribute_offsets);
  }

  attribute_offsets_core.free_buffers();
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
  SCOPED_TIMER_STATS(__func__);

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

BLI_NOINLINE static void delete_tagged_particles(ParticlesState &state)
{
  for (ParticlesContainer *container : state.particle_containers().values()) {
    for (ParticlesBlock *block : container->active_blocks()) {
      delete_tagged_particles_and_reorder(*block);
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

BLI_NOINLINE static void compress_all_containers(ParticlesState &state)
{
  for (ParticlesContainer *container : state.particle_containers().values()) {
    compress_all_blocks(*container);
  }
}

/* Fix state based on description.
 *****************************************************/

BLI_NOINLINE static void ensure_required_containers_exist(ParticlesState &state,
                                                          StepDescription &description)
{
  auto &containers = state.particle_containers();

  for (uint type_id : description.particle_type_ids()) {
    if (!containers.contains(type_id)) {
      ParticlesContainer *container = new ParticlesContainer({}, 1000);
      containers.add_new(type_id, container);
    }
  }
}

BLI_NOINLINE static AttributesInfo build_attribute_info_for_type(ParticleType &type,
                                                                 AttributesInfo &UNUSED(last_info))
{
  SmallSetVector<std::string> byte_attributes = {"Kill State"};
  SmallSetVector<std::string> float_attributes = {"Birth Time"};
  SmallSetVector<std::string> float3_attributes = {};

  byte_attributes.add_multiple(type.byte_attributes());
  float_attributes.add_multiple(type.float_attributes());
  float3_attributes.add_multiple(type.float3_attributes());

  return AttributesInfo(byte_attributes, float_attributes, float3_attributes);
}

BLI_NOINLINE static void ensure_required_attributes_exist(ParticlesState &state,
                                                          StepDescription &description)
{
  auto &containers = state.particle_containers();

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

BLI_NOINLINE static void simulate_all_existing_blocks(ParticlesState &state,
                                                      StepDescription &step_description,
                                                      BlockAllocators &block_allocators,
                                                      TimeSpan time_span)
{
  auto &containers = state.particle_containers();

  SmallVector<ParticlesBlock *> blocks_to_simulate_next;
  for (uint type_id : step_description.particle_type_ids()) {
    ParticlesContainer &container = *containers.lookup(type_id);
    blocks_to_simulate_next.extend(container.active_blocks().to_small_vector());
  }
  simulate_blocks_for_time_span(
      block_allocators, blocks_to_simulate_next, step_description, time_span);
}

BLI_NOINLINE static void create_particles_from_emitters(StepDescription &step_description,
                                                        BlockAllocators &block_allocators,
                                                        TimeSpan time_span)
{
  BlockAllocator &emitter_allocator = block_allocators.get_standalone_allocator();
  for (Emitter *emitter : step_description.emitters()) {
    EmitterInterface interface(emitter_allocator, time_span);
    emitter->emit(interface);
  }
}

BLI_NOINLINE static void emit_and_simulate_particles(ParticlesState &state,
                                                     StepDescription &step_description,
                                                     TimeSpan time_span)
{
  SmallVector<ParticlesBlock *> newly_created_blocks;
  {
    BlockAllocators block_allocators(state);
    simulate_all_existing_blocks(state, step_description, block_allocators, time_span);
    create_particles_from_emitters(step_description, block_allocators, time_span);
    newly_created_blocks = block_allocators.all_allocated_blocks();
  }

  while (newly_created_blocks.size() > 0) {
    BlockAllocators block_allocators(state);
    simulate_blocks_from_birth_to_current_time(
        block_allocators, newly_created_blocks, step_description, time_span.end());
    newly_created_blocks = block_allocators.all_allocated_blocks();
  }
}

void simulate_step(ParticlesState &state, StepDescription &step_description)
{
  TimeSpan time_span(state.m_current_time, step_description.step_duration());
  state.m_current_time = time_span.end();

  ensure_required_containers_exist(state, step_description);
  ensure_required_attributes_exist(state, step_description);

  emit_and_simulate_particles(state, step_description, time_span);

  delete_tagged_particles(state);
  compress_all_containers(state);
}

}  // namespace BParticles
