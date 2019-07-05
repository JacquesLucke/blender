#include "simulate.hpp"
#include "time_span.hpp"

#include "BLI_lazy_init.hpp"
#include "BLI_task.hpp"
#include "BLI_timeit.hpp"

#include "xmmintrin.h"

#define USE_THREADING true
#define BLOCK_SIZE 1000

namespace BParticles {

using BLI::VectorAdaptor;

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

static uint get_max_event_storage_size(ArrayRef<Event *> events)
{
  uint max_size = 0;
  for (Event *event : events) {
    max_size = std::max(max_size, event->storage_size());
  }
  return max_size;
}

BLI_NOINLINE static void find_next_event_per_particle(
    ParticleSet particles,
    AttributeArrays &attribute_offsets,
    ArrayRef<float> durations,
    float end_time,
    ArrayRef<Event *> events,
    EventStorage &r_event_storage,
    ArrayRef<int> r_next_event_indices,
    ArrayRef<float> r_time_factors_to_next_event,
    VectorAdaptor<uint> &r_indices_with_event,
    VectorAdaptor<uint> &r_particle_indices_with_event)
{
  r_next_event_indices.fill(-1);
  r_time_factors_to_next_event.fill(1.0f);

  for (uint event_index = 0; event_index < events.size(); event_index++) {
    SmallVector<uint> triggered_indices;
    SmallVector<float> triggered_time_factors;

    Event *event = events[event_index];
    EventFilterInterface interface(particles,
                                   attribute_offsets,
                                   durations,
                                   end_time,
                                   r_time_factors_to_next_event,
                                   r_event_storage,
                                   triggered_indices,
                                   triggered_time_factors);
    event->filter(interface);

    for (uint i = 0; i < triggered_indices.size(); i++) {
      uint index = triggered_indices[i];
      float time_factor = triggered_time_factors[i];
      BLI_assert(time_factor <= r_time_factors_to_next_event[index]);

      r_next_event_indices[index] = event_index;
      r_time_factors_to_next_event[index] = time_factor;
    }
  }

  for (uint i = 0; i < r_next_event_indices.size(); i++) {
    if (r_next_event_indices[i] != -1) {
      r_indices_with_event.append(i);
      r_particle_indices_with_event.append(particles.get_particle_index(i));
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
      for (uint pindex : particles.range()) {
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
    ArrayRef<uint> indices_with_event,
    ArrayRef<uint> particle_indices_with_event,
    ArrayRef<float> time_factors_to_next_event,
    AttributeArrays attribute_offsets)
{
  BLI_assert(indices_with_event.size() == particle_indices_with_event.size());

  for (uint attribute_index : attribute_offsets.info().float3_attributes()) {
    auto offsets = attribute_offsets.get_float3(attribute_index);

    for (uint i = 0; i < indices_with_event.size(); i++) {
      uint index = indices_with_event[i];
      uint pindex = particle_indices_with_event[i];
      float factor = 1.0f - time_factors_to_next_event[index];
      offsets[pindex] *= factor;
    }
  }
}

BLI_NOINLINE static void find_particle_indices_per_event(
    ArrayRef<uint> indices_with_events,
    ArrayRef<uint> particle_indices_with_events,
    ArrayRef<int> next_event_indices,
    ArrayRef<SmallVector<uint>> r_particles_per_event)
{
  BLI_assert(indices_with_events.size() == particle_indices_with_events.size());
  for (uint i = 0; i < indices_with_events.size(); i++) {
    uint index = indices_with_events[i];
    uint pindex = particle_indices_with_events[i];
    int event_index = next_event_indices[index];
    BLI_assert(event_index >= 0);
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
    VectorAdaptor<uint> &r_unfinished_particle_indices,
    VectorAdaptor<float> &r_remaining_durations)
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

BLI_NOINLINE static void execute_events(BlockAllocator &block_allocator,
                                        ParticlesBlock &block,
                                        ArrayRef<SmallVector<uint>> particle_indices_per_event,
                                        ArrayRef<SmallVector<float>> current_time_per_particle,
                                        ArrayRef<Event *> events,
                                        EventStorage &event_storage,
                                        AttributeArrays attribute_offsets,
                                        float end_time)
{
  BLI_assert(events.size() == particle_indices_per_event.size());
  BLI_assert(events.size() == current_time_per_particle.size());

  for (uint event_index = 0; event_index < events.size(); event_index++) {
    Event *event = events[event_index];
    ParticleSet particles(block, particle_indices_per_event[event_index]);
    if (particles.size() == 0) {
      continue;
    }

    EventExecuteInterface interface(particles,
                                    block_allocator,
                                    current_time_per_particle[event_index],
                                    event_storage,
                                    attribute_offsets,
                                    end_time);
    event->execute(interface);
  }
}

BLI_NOINLINE static void simulate_to_next_event(FixedArrayAllocator &array_allocator,
                                                BlockAllocator &block_allocator,
                                                ParticleSet particles,
                                                AttributeArrays attribute_offsets,
                                                ArrayRef<float> durations,
                                                float end_time,
                                                ArrayRef<Event *> events,
                                                VectorAdaptor<uint> &r_unfinished_particle_indices,
                                                VectorAdaptor<float> &r_remaining_durations)
{
  uint amount = particles.size();

  BLI_assert(array_allocator.array_size() >= amount);
  int *next_event_indices_array = array_allocator.allocate_array<int>();
  float *time_factors_to_next_event_array = array_allocator.allocate_array<float>();
  uint *indices_with_event_array = array_allocator.allocate_array<uint>();
  uint *particle_indices_with_event_array = array_allocator.allocate_array<uint>();

  VectorAdaptor<int> next_event_indices(next_event_indices_array, amount, amount);
  VectorAdaptor<float> time_factors_to_next_event(
      time_factors_to_next_event_array, amount, amount);
  VectorAdaptor<uint> indices_with_event(indices_with_event_array, amount);
  VectorAdaptor<uint> particle_indices_with_event(particle_indices_with_event_array, amount);

  uint max_event_storage_size = std::max(get_max_event_storage_size(events), 1u);
  void *event_storage_array = array_allocator.allocate_array(max_event_storage_size);
  EventStorage event_storage(event_storage_array, max_event_storage_size);

  find_next_event_per_particle(particles,
                               attribute_offsets,
                               durations,
                               end_time,
                               events,
                               event_storage,
                               next_event_indices,
                               time_factors_to_next_event,
                               indices_with_event,
                               particle_indices_with_event);

  forward_particles_to_next_event_or_end(particles, attribute_offsets, time_factors_to_next_event);

  update_remaining_attribute_offsets(indices_with_event,
                                     particle_indices_with_event,
                                     time_factors_to_next_event,
                                     attribute_offsets);

  SmallVector<SmallVector<uint>> particles_per_event(events.size());
  find_particle_indices_per_event(
      indices_with_event, particle_indices_with_event, next_event_indices, particles_per_event);

  SmallVector<SmallVector<float>> current_time_per_particle(events.size());
  compute_current_time_per_particle(indices_with_event,
                                    durations,
                                    end_time,
                                    next_event_indices,
                                    time_factors_to_next_event,
                                    current_time_per_particle);

  execute_events(block_allocator,
                 particles.block(),
                 particles_per_event,
                 current_time_per_particle,
                 events,
                 event_storage,
                 attribute_offsets,
                 end_time);

  find_unfinished_particles(indices_with_event,
                            particles.indices(),
                            time_factors_to_next_event,
                            durations,
                            particles.attributes().get_byte("Kill State"),
                            r_unfinished_particle_indices,
                            r_remaining_durations);

  array_allocator.deallocate_array(next_event_indices_array);
  array_allocator.deallocate_array(time_factors_to_next_event_array);
  array_allocator.deallocate_array(indices_with_event_array);
  array_allocator.deallocate_array(particle_indices_with_event_array);
  array_allocator.deallocate_array(event_storage_array, max_event_storage_size);
}

BLI_NOINLINE static void simulate_with_max_n_events(
    uint max_events,
    FixedArrayAllocator &array_allocator,
    BlockAllocator &block_allocator,
    ParticlesBlock &block,
    AttributeArrays attribute_offsets,
    ArrayRef<float> durations,
    float end_time,
    ArrayRef<Event *> events,
    VectorAdaptor<uint> &r_unfinished_particle_indices)
{
  BLI_assert(array_allocator.array_size() >= block.active_amount());
  uint *indices_A = array_allocator.allocate_array<uint>();
  uint *indices_B = array_allocator.allocate_array<uint>();
  float *durations_A = array_allocator.allocate_array<float>();
  float *durations_B = array_allocator.allocate_array<float>();

  /* Handle first event separately to be able to use the static number range. */
  uint amount_left = block.active_amount();

  {
    VectorAdaptor<uint> indices_output(indices_A, amount_left);
    VectorAdaptor<float> durations_output(durations_A, amount_left);
    simulate_to_next_event(array_allocator,
                           block_allocator,
                           ParticleSet(block, static_number_range_ref(0, amount_left)),
                           attribute_offsets,
                           durations,
                           end_time,
                           events,
                           indices_output,
                           durations_output);
    BLI_assert(indices_output.size() == durations_output.size());
    amount_left = indices_output.size();
  }

  for (uint iteration = 0; iteration < max_events - 1 && amount_left > 0; iteration++) {
    VectorAdaptor<uint> indices_input(indices_A, amount_left, amount_left);
    VectorAdaptor<uint> indices_output(indices_B, amount_left, 0);
    VectorAdaptor<float> durations_input(durations_A, amount_left, amount_left);
    VectorAdaptor<float> durations_output(durations_B, amount_left, 0);

    simulate_to_next_event(array_allocator,
                           block_allocator,
                           ParticleSet(block, indices_input),
                           attribute_offsets,
                           durations_input,
                           end_time,
                           events,
                           indices_output,
                           durations_output);
    BLI_assert(indices_output.size() == durations_output.size());

    amount_left = indices_output.size();
    std::swap(indices_A, indices_B);
    std::swap(durations_A, durations_B);
  }

  for (uint i = 0; i < amount_left; i++) {
    r_unfinished_particle_indices.append(indices_A[i]);
  }

  array_allocator.deallocate_array(indices_A);
  array_allocator.deallocate_array(indices_B);
  array_allocator.deallocate_array(durations_A);
  array_allocator.deallocate_array(durations_B);
}

BLI_NOINLINE static void add_float3_arrays(ArrayRef<float3> base, ArrayRef<float3> values)
{
  /* I'm just testing the impact of vectorization here.
   * This should eventually be moved to another place. */
  BLI_assert(base.size() == values.size());
  BLI_assert(POINTER_AS_UINT(base.begin()) % 16 == 0);
  BLI_assert(POINTER_AS_UINT(values.begin()) % 16 == 0);

  float *base_start = (float *)base.begin();
  float *values_start = (float *)values.begin();
  uint total_size = base.size() * 3;
  uint overshoot = total_size % 4;
  uint vectorized_size = total_size - overshoot;

  /* Twice as fast in my test than the normal loop.
   * The compiler did not vectorize it, maybe for compatibility? */
  for (uint i = 0; i < vectorized_size; i += 4) {
    __m128 a = _mm_load_ps(base_start + i);
    __m128 b = _mm_load_ps(values_start + i);
    __m128 result = _mm_add_ps(a, b);
    _mm_store_ps(base_start + i, result);
  }

  for (uint i = vectorized_size; i < total_size; i++) {
    base_start[i] += values_start[i];
  }
}

BLI_NOINLINE static void apply_remaining_offsets(ParticleSet particles,
                                                 AttributeArrays attribute_offsets)
{
  for (uint attribute_index : attribute_offsets.info().float3_attributes()) {
    StringRef name = attribute_offsets.info().name_of(attribute_index);

    auto values = particles.attributes().get_float3(name);
    auto offsets = attribute_offsets.get_float3(attribute_index);

    if (particles.indices_are_trivial()) {
      add_float3_arrays(values.take_front(particles.size()), offsets.take_front(particles.size()));
    }
    else {
      for (uint pindex : particles.indices()) {
        values[pindex] += offsets[pindex];
      }
    }
  }
}

BLI_NOINLINE static void simulate_block(FixedArrayAllocator &array_allocator,
                                        BlockAllocator &block_allocator,
                                        ParticlesBlock &block,
                                        ParticleType &particle_type,
                                        ArrayRef<float> durations,
                                        float end_time)
{
  uint amount = block.active_amount();
  BLI_assert(amount == durations.size());

  Integrator &integrator = particle_type.integrator();
  AttributesInfo &offsets_info = integrator.offset_attributes_info();
  AttributeArraysCore attribute_offsets_core = AttributeArraysCore::NewWithArrayAllocator(
      offsets_info, array_allocator);
  AttributeArrays attribute_offsets = attribute_offsets_core.slice_all().slice(0, amount);

  IntegratorInterface interface(block, durations, attribute_offsets);
  integrator.integrate(interface);

  ArrayRef<Event *> events = particle_type.events();

  if (events.size() == 0) {
    ParticleSet all_particles_in_block(block, static_number_range_ref(block.active_range()));
    apply_remaining_offsets(all_particles_in_block, attribute_offsets);
  }
  else {
    uint *indices_array = array_allocator.allocate_array<uint>();
    VectorAdaptor<uint> unfinished_particle_indices(indices_array, amount);

    simulate_with_max_n_events(1,
                               array_allocator,
                               block_allocator,
                               block,
                               attribute_offsets,
                               durations,
                               end_time,
                               events,
                               unfinished_particle_indices);

    /* Not sure yet, if this really should be done. */
    if (unfinished_particle_indices.size() > 0) {
      ParticleSet remaining_particles(block, unfinished_particle_indices);
      apply_remaining_offsets(remaining_particles, attribute_offsets);
    }

    array_allocator.deallocate_array(indices_array);
  }

  attribute_offsets_core.deallocate_in_array_allocator(array_allocator);
}

class BlockAllocators {
 private:
  ParticlesState &m_state;
  SmallVector<BlockAllocator *> m_allocators;

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

  BlockAllocator &new_allocator()
  {
    BlockAllocator *new_allocator = new BlockAllocator(m_state);
    m_allocators.append(new_allocator);
    return *new_allocator;
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

struct ThreadLocalData {
  FixedArrayAllocator array_allocator;
  BlockAllocator &block_allocator;

  ThreadLocalData(uint block_size, BlockAllocator &block_allocator)
      : array_allocator(block_size), block_allocator(block_allocator)
  {
  }
};

BLI_NOINLINE static void simulate_blocks_for_time_span(BlockAllocators &block_allocators,
                                                       ArrayRef<ParticlesBlock *> blocks,
                                                       StepDescription &step_description,
                                                       TimeSpan time_span)
{
  if (blocks.size() == 0) {
    return;
  }

  uint block_size = blocks[0]->container().block_size();
  SmallVector<float> all_durations(block_size);
  all_durations.fill(time_span.duration());

  BLI::Task::parallel_array_elements(
      blocks,
      /* Process individual element. */
      [&step_description, &all_durations, time_span](ParticlesBlock *block,
                                                     ThreadLocalData *local_data) {
        ParticlesState &state = local_data->block_allocator.particles_state();
        StringRef particle_type_name = state.particle_container_id(block->container());
        ParticleType &particle_type = step_description.particle_type(particle_type_name);

        simulate_block(local_data->array_allocator,
                       local_data->block_allocator,
                       *block,
                       particle_type,
                       ArrayRef<float>(all_durations).take_back(block->active_amount()),
                       time_span.end());
      },
      /* Create thread-local data. */
      [&block_allocators]() {
        return new ThreadLocalData(BLOCK_SIZE, block_allocators.new_allocator());
      },
      /* Free thread-local data. */
      [](ThreadLocalData *local_data) { delete local_data; },
      USE_THREADING);
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

  BLI::Task::parallel_array_elements(
      blocks,
      /* Process individual element. */
      [&step_description, end_time](ParticlesBlock *block, ThreadLocalData *local_data) {
        ParticlesState &state = local_data->block_allocator.particles_state();
        StringRef particle_type_id = state.particle_container_id(block->container());
        ParticleType &particle_type = step_description.particle_type(particle_type_id);

        uint active_amount = block->active_amount();
        SmallVector<float> durations(active_amount);
        auto birth_times = block->attributes().get_float("Birth Time");
        for (uint i = 0; i < active_amount; i++) {
          durations[i] = end_time - birth_times[i];
        }
        simulate_block(local_data->array_allocator,
                       local_data->block_allocator,
                       *block,
                       particle_type,
                       durations,
                       end_time);
      },
      /* Create thread-local data. */
      [&block_allocators]() {
        return new ThreadLocalData(BLOCK_SIZE, block_allocators.new_allocator());
      },
      /* Free thread-local data. */
      [](ThreadLocalData *local_data) { delete local_data; },
      USE_THREADING);
}

BLI_NOINLINE static SmallVector<ParticlesBlock *> get_all_blocks(
    ParticlesState &state, ArrayRef<std::string> particle_type_names)
{
  SmallVector<ParticlesBlock *> blocks;
  for (StringRef particle_type_name : particle_type_names) {
    ParticlesContainer &container = state.particle_container(particle_type_name);
    for (ParticlesBlock *block : container.active_blocks()) {
      blocks.append(block);
    }
  }
  return blocks;
}

BLI_NOINLINE static void delete_tagged_particles_and_reorder(ParticlesBlock &block)
{
  auto kill_states = block.attributes().get_byte("Kill State");

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

BLI_NOINLINE static void delete_tagged_particles(ParticlesState &state,
                                                 ArrayRef<std::string> particle_type_names)
{
  SmallVector<ParticlesBlock *> blocks = get_all_blocks(state, particle_type_names);

  BLI::Task::parallel_array_elements(
      ArrayRef<ParticlesBlock *>(blocks),
      [](ParticlesBlock *block) { delete_tagged_particles_and_reorder(*block); },
      USE_THREADING);
}

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

BLI_NOINLINE static void ensure_required_containers_exist(ParticlesState &state,
                                                          StepDescription &description)
{
  auto &containers = state.particle_containers();

  for (std::string &type_name : description.particle_type_names()) {
    if (!containers.contains(type_name)) {
      ParticlesContainer *container = new ParticlesContainer({}, BLOCK_SIZE);
      containers.add_new(type_name, container);
    }
  }
}

BLI_NOINLINE static AttributesInfo build_attribute_info_for_type(ParticleType &type,
                                                                 AttributesInfo &UNUSED(last_info))
{
  TypeAttributeInterface interface;
  type.attributes(interface);

  SmallSetVector<std::string> byte_attributes = {"Kill State"};
  SmallSetVector<std::string> float_attributes = {"Birth Time"};
  SmallSetVector<std::string> float3_attributes = {};

  for (uint i = 0; i < interface.names().size(); i++) {
    std::string &name = interface.names()[i];
    switch (interface.types()[i]) {
      case AttributeType::Byte:
        byte_attributes.add(name);
        break;
      case AttributeType::Float:
        float_attributes.add(name);
        break;
      case AttributeType::Float3:
        float3_attributes.add(name);
        break;
    }
  }

  return AttributesInfo(byte_attributes, float_attributes, float3_attributes);
}

BLI_NOINLINE static void ensure_required_attributes_exist(ParticlesState &state,
                                                          StepDescription &description)
{
  auto &containers = state.particle_containers();

  for (std::string &type_name : description.particle_type_names()) {
    ParticleType &type = description.particle_type(type_name);
    ParticlesContainer &container = *containers.lookup(type_name);

    AttributesInfo new_attributes_info = build_attribute_info_for_type(
        type, container.attributes_info());
    container.update_attributes(new_attributes_info);
  }
}

BLI_NOINLINE static void simulate_all_existing_blocks(ParticlesState &state,
                                                      StepDescription &step_description,
                                                      BlockAllocators &block_allocators,
                                                      TimeSpan time_span)
{
  SmallVector<ParticlesBlock *> blocks = get_all_blocks(state,
                                                        step_description.particle_type_names());
  simulate_blocks_for_time_span(block_allocators, blocks, step_description, time_span);
}

BLI_NOINLINE static void create_particles_from_emitters(StepDescription &step_description,
                                                        BlockAllocators &block_allocators,
                                                        TimeSpan time_span)
{
  BlockAllocator &emitter_allocator = block_allocators.new_allocator();
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
  TimeSpan time_span(state.current_time(), step_description.step_duration());
  state.current_time() = time_span.end();

  ensure_required_containers_exist(state, step_description);
  ensure_required_attributes_exist(state, step_description);

  emit_and_simulate_particles(state, step_description, time_span);

  delete_tagged_particles(state, step_description.particle_type_names());
  compress_all_containers(state);
}

}  // namespace BParticles
