#include "simulate.hpp"
#include "time_span.hpp"

#include "BLI_lazy_init.hpp"
#include "BLI_task.hpp"
#include "BLI_timeit.hpp"

#include "xmmintrin.h"

#define USE_THREADING true

namespace BParticles {

using BLI::VectorAdaptor;

static uint get_max_event_storage_size(ArrayRef<Event *> events)
{
  uint max_size = 0;
  for (Event *event : events) {
    max_size = std::max(max_size, event->storage_size());
  }
  return max_size;
}

BLI_NOINLINE static void find_next_event_per_particle(BlockStepData &step_data,
                                                      ArrayRef<uint> pindices,
                                                      EventStorage &r_event_storage,
                                                      ArrayRef<int> r_next_event_indices,
                                                      ArrayRef<float> r_time_factors_to_next_event,
                                                      VectorAdaptor<uint> &r_pindices_with_event)
{
  r_next_event_indices.fill_indices(pindices, -1);
  r_time_factors_to_next_event.fill_indices(pindices, 1.0f);

  ArrayRef<Event *> events = step_data.particle_type.events();

  for (uint event_index = 0; event_index < events.size(); event_index++) {
    SmallVector<uint> triggered_pindices;
    SmallVector<float> triggered_time_factors;

    Event *event = events[event_index];
    EventFilterInterface interface(step_data,
                                   pindices,
                                   r_time_factors_to_next_event,
                                   r_event_storage,
                                   triggered_pindices,
                                   triggered_time_factors);
    event->filter(interface);

    for (uint i = 0; i < triggered_pindices.size(); i++) {
      uint pindex = triggered_pindices[i];
      float time_factor = triggered_time_factors[i];
      BLI_assert(time_factor <= r_time_factors_to_next_event[pindex]);

      r_next_event_indices[pindex] = event_index;
      r_time_factors_to_next_event[pindex] = time_factor;
    }
  }

  for (uint pindex : pindices) {
    if (r_next_event_indices[pindex] != -1) {
      r_pindices_with_event.append(pindex);
    }
  }
}

BLI_NOINLINE static void forward_particles_to_next_event_or_end(
    BlockStepData &step_data, ArrayRef<uint> pindices, ArrayRef<float> time_factors_to_next_event)
{
  OffsetHandlerInterface interface(step_data, pindices, time_factors_to_next_event);
  for (OffsetHandler *handler : step_data.particle_type.offset_handlers()) {
    handler->execute(interface);
  }

  ParticleSet particles(step_data.block, pindices);

  auto attribute_offsets = step_data.attribute_offsets;
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
      for (uint pindex : particles.pindices()) {
        float time_factor = time_factors_to_next_event[pindex];
        values[pindex] += time_factor * offsets[pindex];
      }
    }
  }
}

BLI_NOINLINE static void update_remaining_attribute_offsets(
    ArrayRef<uint> pindices_with_event,
    ArrayRef<float> time_factors_to_next_event,
    AttributeArrays attribute_offsets)
{
  for (uint attribute_index : attribute_offsets.info().float3_attributes()) {
    auto offsets = attribute_offsets.get_float3(attribute_index);

    for (uint pindex : pindices_with_event) {
      float factor = 1.0f - time_factors_to_next_event[pindex];
      offsets[pindex] *= factor;
    }
  }
}

BLI_NOINLINE static void update_remaining_durations(ArrayRef<uint> pindices_with_event,
                                                    ArrayRef<float> time_factors_to_next_event,
                                                    ArrayRef<float> remaining_durations)
{
  for (uint pindex : pindices_with_event) {
    remaining_durations[pindex] *= (1.0f - time_factors_to_next_event[pindex]);
  }
}

BLI_NOINLINE static void find_pindices_per_event(ArrayRef<uint> pindices_with_events,
                                                 ArrayRef<int> next_event_indices,
                                                 ArrayRef<SmallVector<uint>> r_particles_per_event)
{
  for (uint pindex : pindices_with_events) {
    int event_index = next_event_indices[pindex];
    BLI_assert(event_index >= 0);
    r_particles_per_event[event_index].append(pindex);
  }
}

BLI_NOINLINE static void compute_current_time_per_particle(ArrayRef<uint> pindices_with_event,
                                                           ArrayRef<float> remaining_durations,
                                                           float end_time,
                                                           ArrayRef<float> r_current_times)
{
  for (uint pindex : pindices_with_event) {
    r_current_times[pindex] = end_time - remaining_durations[pindex];
  }
}

BLI_NOINLINE static void find_unfinished_particles(ArrayRef<uint> pindices_with_event,
                                                   ArrayRef<float> time_factors_to_next_event,
                                                   ArrayRef<uint8_t> kill_states,
                                                   VectorAdaptor<uint> &r_unfinished_pindices)
{
  for (uint pindex : pindices_with_event) {
    if (kill_states[pindex] == 0) {
      float time_factor = time_factors_to_next_event[pindex];

      if (time_factor < 1.0f) {
        r_unfinished_pindices.append(pindex);
      }
    }
  }
}

BLI_NOINLINE static void execute_events(BlockStepData &step_data,
                                        ArrayRef<SmallVector<uint>> pindices_per_event,
                                        ArrayRef<float> current_times,
                                        EventStorage &event_storage)
{
  ArrayRef<Event *> events = step_data.particle_type.events();

  BLI_assert(events.size() == pindices_per_event.size());

  for (uint event_index = 0; event_index < events.size(); event_index++) {
    Event *event = events[event_index];
    ArrayRef<uint> pindices = pindices_per_event[event_index];

    if (pindices.size() == 0) {
      continue;
    }

    EventExecuteInterface interface(step_data, pindices, current_times, event_storage);
    event->execute(interface);
  }
}

BLI_NOINLINE static void simulate_to_next_event(BlockStepData &step_data,
                                                ArrayRef<uint> pindices,
                                                VectorAdaptor<uint> &r_unfinished_pindices)
{
  ArrayRef<Event *> events = step_data.particle_type.events();

  ArrayAllocator::Array<int> next_event_indices(step_data.array_allocator);
  ArrayAllocator::Array<float> time_factors_to_next_event(step_data.array_allocator);
  ArrayAllocator::Vector<uint> pindices_with_event(step_data.array_allocator);

  uint max_event_storage_size = std::max(get_max_event_storage_size(events), 1u);
  auto event_storage_array = step_data.array_allocator.allocate_scoped(max_event_storage_size);
  EventStorage event_storage(event_storage_array, max_event_storage_size);

  find_next_event_per_particle(step_data,
                               pindices,
                               event_storage,
                               next_event_indices,
                               time_factors_to_next_event,
                               pindices_with_event);

  forward_particles_to_next_event_or_end(step_data, pindices, time_factors_to_next_event);

  update_remaining_attribute_offsets(
      pindices_with_event, time_factors_to_next_event, step_data.attribute_offsets);

  update_remaining_durations(
      pindices_with_event, time_factors_to_next_event, step_data.remaining_durations);

  SmallVector<SmallVector<uint>> particles_per_event(events.size());
  find_pindices_per_event(pindices_with_event, next_event_indices, particles_per_event);

  ArrayAllocator::Array<float> current_times(step_data.array_allocator);
  compute_current_time_per_particle(
      pindices_with_event, step_data.remaining_durations, step_data.step_end_time, current_times);

  execute_events(step_data, particles_per_event, current_times, event_storage);

  find_unfinished_particles(pindices_with_event,
                            time_factors_to_next_event,
                            step_data.block.attributes().get_byte("Kill State"),
                            r_unfinished_pindices);
}

BLI_NOINLINE static void simulate_with_max_n_events(BlockStepData &step_data,
                                                    uint max_events,
                                                    VectorAdaptor<uint> &r_unfinished_pindices)
{
  BLI_assert(step_data.array_allocator.array_size() >= step_data.block.active_amount());
  auto pindices_A = step_data.array_allocator.allocate_scoped<uint>();
  auto pindices_B = step_data.array_allocator.allocate_scoped<uint>();

  uint amount_left = step_data.block.active_amount();

  {
    /* Handle first event separately to be able to use the static number range. */
    VectorAdaptor<uint> pindices_output(pindices_A, amount_left);
    simulate_to_next_event(step_data, Range<uint>(0, amount_left).as_array_ref(), pindices_output);
    amount_left = pindices_output.size();
  }

  for (uint iteration = 0; iteration < max_events - 1 && amount_left > 0; iteration++) {
    VectorAdaptor<uint> pindices_input(pindices_A, amount_left, amount_left);
    VectorAdaptor<uint> pindices_output(pindices_B, amount_left, 0);

    simulate_to_next_event(step_data, pindices_input, pindices_output);
    amount_left = pindices_output.size();
    std::swap(pindices_A, pindices_B);
  }

  for (uint i = 0; i < amount_left; i++) {
    r_unfinished_pindices.append(pindices_A[i]);
  }
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

BLI_NOINLINE static void apply_remaining_offsets(BlockStepData &step_data, ArrayRef<uint> pindices)
{
  auto handlers = step_data.particle_type.offset_handlers();
  if (handlers.size() > 0) {
    ArrayAllocator::Array<float> time_factors(step_data.array_allocator);
    for (uint pindex : pindices) {
      time_factors[pindex] = 1.0f;
    }

    OffsetHandlerInterface interface(step_data, pindices, time_factors);
    for (OffsetHandler *handler : handlers) {
      handler->execute(interface);
    }
  }

  auto attribute_offsets = step_data.attribute_offsets;
  ParticleSet particles(step_data.block, pindices);

  for (uint attribute_index : attribute_offsets.info().float3_attributes()) {
    StringRef name = attribute_offsets.info().name_of(attribute_index);

    auto values = particles.attributes().get_float3(name);
    auto offsets = attribute_offsets.get_float3(attribute_index);

    if (particles.indices_are_trivial()) {
      add_float3_arrays(values.take_front(particles.size()), offsets.take_front(particles.size()));
    }
    else {
      for (uint pindex : particles.pindices()) {
        values[pindex] += offsets[pindex];
      }
    }
  }
}

BLI_NOINLINE static void simulate_block(ArrayAllocator &array_allocator,
                                        ParticleAllocator &particle_allocator,
                                        ParticlesBlock &block,
                                        ParticleType &particle_type,
                                        ArrayRef<float> remaining_durations,
                                        float end_time)
{
  uint amount = block.active_amount();
  BLI_assert(amount == remaining_durations.size());

  Integrator &integrator = particle_type.integrator();
  AttributesInfo &offsets_info = integrator.offset_attributes_info();
  AttributeArraysCore attribute_offsets_core = AttributeArraysCore::NewWithArrayAllocator(
      offsets_info, array_allocator);
  AttributeArrays attribute_offsets = attribute_offsets_core.slice_all().slice(0, amount);

  IntegratorInterface interface(block, remaining_durations, array_allocator, attribute_offsets);
  integrator.integrate(interface);

  BlockStepData step_data = {array_allocator,
                             particle_allocator,
                             block,
                             particle_type,
                             attribute_offsets,
                             remaining_durations,
                             end_time};

  if (particle_type.events().size() == 0) {
    apply_remaining_offsets(step_data, block.active_range().as_array_ref());
  }
  else {
    auto indices_array = array_allocator.allocate_scoped<uint>();
    VectorAdaptor<uint> unfinished_pindices(indices_array, amount);

    simulate_with_max_n_events(step_data, 10, unfinished_pindices);

    /* Not sure yet, if this really should be done. */
    if (unfinished_pindices.size() > 0) {
      apply_remaining_offsets(step_data, unfinished_pindices);
    }
  }

  attribute_offsets_core.deallocate_in_array_allocator(array_allocator);
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

class ParticleAllocators {
 private:
  ParticlesState &m_state;
  SmallVector<std::unique_ptr<ParticleAllocator>> m_allocators;

 public:
  ParticleAllocators(ParticlesState &state) : m_state(state)
  {
  }

  ParticleAllocator &new_allocator()
  {
    ParticleAllocator *allocator = new ParticleAllocator(m_state);
    m_allocators.append(std::unique_ptr<ParticleAllocator>(allocator));
    return *allocator;
  }

  SmallVector<ParticlesBlock *> gather_allocated_blocks()
  {
    SmallVector<ParticlesBlock *> blocks;
    for (auto &allocator : m_allocators) {
      blocks.extend(allocator->allocated_blocks());
    }
    return blocks;
  }
};

struct ThreadLocalData {
  ArrayAllocator array_allocator;
  ParticleAllocator &particle_allocator;

  ThreadLocalData(uint block_size, ParticleAllocator &particle_allocator)
      : array_allocator(block_size), particle_allocator(particle_allocator)
  {
  }
};

BLI_NOINLINE static void simulate_blocks_for_time_span(ParticleAllocators &block_allocators,
                                                       ArrayRef<ParticlesBlock *> blocks,
                                                       StepDescription &step_description,
                                                       TimeSpan time_span,
                                                       uint max_block_size)
{
  if (blocks.size() == 0) {
    return;
  }

  BLI::Task::parallel_array_elements(
      blocks,
      /* Process individual element. */
      [&step_description, time_span](ParticlesBlock *block, ThreadLocalData *local_data) {
        ParticlesState &state = local_data->particle_allocator.particles_state();
        StringRef particle_type_name = state.particle_container_name(block->container());
        ParticleType &particle_type = *step_description.particle_types().lookup(
            particle_type_name);

        ArrayAllocator &array_allocator = local_data->array_allocator;
        ArrayAllocator::Array<float> remaining_durations(array_allocator, block->active_amount());
        ArrayRef<float>(remaining_durations).fill(time_span.duration());

        simulate_block(local_data->array_allocator,
                       local_data->particle_allocator,
                       *block,
                       particle_type,
                       remaining_durations,
                       time_span.end());

        delete_tagged_particles_and_reorder(*block);
      },
      /* Create thread-local data. */
      [&block_allocators, max_block_size]() {
        return new ThreadLocalData(max_block_size, block_allocators.new_allocator());
      },
      /* Free thread-local data. */
      [](ThreadLocalData *local_data) { delete local_data; },
      USE_THREADING);
}

BLI_NOINLINE static void simulate_blocks_from_birth_to_current_time(
    ParticleAllocators &block_allocators,
    ArrayRef<ParticlesBlock *> blocks,
    StepDescription &step_description,
    float end_time,
    uint max_block_size)
{
  if (blocks.size() == 0) {
    return;
  }

  BLI::Task::parallel_array_elements(
      blocks,
      /* Process individual element. */
      [&step_description, end_time](ParticlesBlock *block, ThreadLocalData *local_data) {
        ParticlesState &state = local_data->particle_allocator.particles_state();
        StringRef particle_type_name = state.particle_container_name(block->container());
        ParticleType &particle_type = *step_description.particle_types().lookup(
            particle_type_name);

        uint active_amount = block->active_amount();
        SmallVector<float> durations(active_amount);
        auto birth_times = block->attributes().get_float("Birth Time");
        for (uint i = 0; i < active_amount; i++) {
          durations[i] = end_time - birth_times[i];
        }
        simulate_block(local_data->array_allocator,
                       local_data->particle_allocator,
                       *block,
                       particle_type,
                       durations,
                       end_time);

        delete_tagged_particles_and_reorder(*block);
      },
      /* Create thread-local data. */
      [&block_allocators, max_block_size]() {
        return new ThreadLocalData(max_block_size, block_allocators.new_allocator());
      },
      /* Free thread-local data. */
      [](ThreadLocalData *local_data) { delete local_data; },
      USE_THREADING);
}

BLI_NOINLINE static SmallVector<ParticlesBlock *> get_all_blocks(ParticlesState &state,
                                                                 StepDescription &step_description)
{
  SmallVector<ParticlesBlock *> blocks;
  for (auto particle_type_name : step_description.particle_types().keys()) {
    ParticlesContainer &container = state.particle_container(particle_type_name);
    for (ParticlesBlock *block : container.active_blocks()) {
      blocks.append(block);
    }
  }
  return blocks;
}

BLI_NOINLINE static void compress_all_blocks(ParticlesContainer &container)
{
  SmallVector<ParticlesBlock *> blocks = container.active_blocks();
  ParticlesBlock::Compress(blocks);

  for (ParticlesBlock *block : blocks) {
    if (block->is_empty()) {
      container.release_block(*block);
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

  for (std::string type_name : description.particle_types().keys()) {
    if (!containers.contains(type_name)) {
      ParticlesContainer *container = new ParticlesContainer({}, 100);
      containers.add_new(type_name, container);
    }
  }
}

BLI_NOINLINE static AttributesInfo build_attribute_info_for_type(ParticleType &type,
                                                                 AttributesInfo &UNUSED(last_info))
{
  AttributesDeclaration builder;
  type.attributes(builder);

  for (Event *event : type.events()) {
    event->attributes(builder);
  }

  builder.add_byte("Kill State", 0);
  builder.add_float("Birth Time", 0);

  return AttributesInfo(builder);
}

BLI_NOINLINE static void ensure_required_attributes_exist(ParticlesState &state,
                                                          StepDescription &description)
{
  auto &containers = state.particle_containers();

  for (auto item : description.particle_types().items()) {
    ParticlesContainer &container = *containers.lookup(item.key);

    AttributesInfo new_attributes_info = build_attribute_info_for_type(
        *item.value, container.attributes_info());
    container.update_attributes(new_attributes_info);
  }
}

BLI_NOINLINE static void simulate_all_existing_blocks(ParticlesState &state,
                                                      StepDescription &step_description,
                                                      ParticleAllocators &block_allocators,
                                                      TimeSpan time_span,
                                                      uint max_block_size)
{
  SmallVector<ParticlesBlock *> blocks = get_all_blocks(state, step_description);
  simulate_blocks_for_time_span(
      block_allocators, blocks, step_description, time_span, max_block_size);
}

BLI_NOINLINE static void create_particles_from_emitters(StepDescription &step_description,
                                                        ParticleAllocators &block_allocators,
                                                        TimeSpan time_span,
                                                        uint max_block_size)
{
  ArrayAllocator array_allocator(max_block_size);
  ParticleAllocator &emitter_allocator = block_allocators.new_allocator();
  for (Emitter *emitter : step_description.emitters()) {
    EmitterInterface interface(emitter_allocator, array_allocator, time_span);
    emitter->emit(interface);
  }
}

BLI_NOINLINE static uint get_max_block_size(ParticlesState &state)
{
  uint max_size = 0;
  for (auto *container : state.particle_containers().values()) {
    max_size = std::max(max_size, container->block_size());
  }
  return max_size;
}

BLI_NOINLINE static void emit_and_simulate_particles(ParticlesState &state,
                                                     StepDescription &step_description,
                                                     TimeSpan time_span)
{
  uint max_block_size = get_max_block_size(state);

  SmallVector<ParticlesBlock *> newly_created_blocks;
  {
    ParticleAllocators block_allocators(state);
    simulate_all_existing_blocks(
        state, step_description, block_allocators, time_span, max_block_size);
    create_particles_from_emitters(step_description, block_allocators, time_span, max_block_size);
    newly_created_blocks = block_allocators.gather_allocated_blocks();
  }

  while (newly_created_blocks.size() > 0) {
    ParticleAllocators block_allocators(state);
    simulate_blocks_from_birth_to_current_time(
        block_allocators, newly_created_blocks, step_description, time_span.end(), max_block_size);
    newly_created_blocks = block_allocators.gather_allocated_blocks();
  }
}

void simulate_step(ParticlesState &state, StepDescription &step_description)
{
  float start_time = state.current_time();
  state.increase_time(step_description.step_duration());
  TimeSpan time_span(start_time, step_description.step_duration());

  ensure_required_containers_exist(state, step_description);
  ensure_required_attributes_exist(state, step_description);

  emit_and_simulate_particles(state, step_description, time_span);

  compress_all_containers(state);
}

}  // namespace BParticles
