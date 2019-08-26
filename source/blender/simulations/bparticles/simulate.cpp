
#include "BLI_lazy_init.hpp"
#include "BLI_task.hpp"
#include "BLI_timeit.hpp"
#include "BLI_array.hpp"
#include "BLI_vector_adaptor.hpp"

#include "simulate.hpp"
#include "time_span.hpp"
#include "xmmintrin.h"

#define USE_THREADING true

namespace BParticles {

using BLI::TemporaryArray;
using BLI::TemporaryVector;
using BLI::VectorAdaptor;

static uint get_max_event_storage_size(ArrayRef<Event *> events)
{
  uint max_size = 0;
  for (Event *event : events) {
    max_size = std::max(max_size, event->storage_size());
  }
  return max_size;
}

BLI_NOINLINE static void find_next_event_per_particle(
    BlockStepData &step_data,
    ArrayRef<uint> pindices,
    ArrayRef<Event *> events,
    EventStorage &r_event_storage,
    MutableArrayRef<int> r_next_event_indices,
    MutableArrayRef<float> r_time_factors_to_next_event,
    TemporaryVector<uint> &r_pindices_with_event)
{
  r_next_event_indices.fill_indices(pindices, -1);
  r_time_factors_to_next_event.fill_indices(pindices, 1.0f);

  for (uint event_index = 0; event_index < events.size(); event_index++) {
    Vector<uint> triggered_pindices;
    Vector<float> triggered_time_factors;

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
    BlockStepData &step_data,
    ArrayRef<uint> pindices,
    ArrayRef<float> time_factors_to_next_event,
    ArrayRef<OffsetHandler *> offset_handlers)
{
  OffsetHandlerInterface interface(step_data, pindices, time_factors_to_next_event);
  for (OffsetHandler *handler : offset_handlers) {
    handler->execute(interface);
  }

  ParticleSet particles(step_data.attributes, pindices);

  auto attribute_offsets = step_data.attribute_offsets;
  for (uint attribute_index : attribute_offsets.info().attribute_indices()) {
    StringRef name = attribute_offsets.info().name_of(attribute_index);

    /* Only vectors can be integrated for now. */
    auto values = particles.attributes().get<float3>(name);
    auto offsets = attribute_offsets.get<float3>(attribute_index);

    if (particles.pindices_are_trivial()) {
      for (uint pindex : particles.trivial_pindices()) {
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
  for (uint attribute_index : attribute_offsets.info().attribute_indices()) {
    /* Only vectors can be integrated for now. */
    auto offsets = attribute_offsets.get<float3>(attribute_index);

    for (uint pindex : pindices_with_event) {
      float factor = 1.0f - time_factors_to_next_event[pindex];
      offsets[pindex] *= factor;
    }
  }
}

BLI_NOINLINE static void update_remaining_durations(ArrayRef<uint> pindices_with_event,
                                                    ArrayRef<float> time_factors_to_next_event,
                                                    MutableArrayRef<float> remaining_durations)
{
  for (uint pindex : pindices_with_event) {
    remaining_durations[pindex] *= (1.0f - time_factors_to_next_event[pindex]);
  }
}

BLI_NOINLINE static void find_pindices_per_event(
    ArrayRef<uint> pindices_with_events,
    ArrayRef<int> next_event_indices,
    MutableArrayRef<Vector<uint>> r_particles_per_event)
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
                                                           MutableArrayRef<float> r_current_times)
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
                                        ArrayRef<Vector<uint>> pindices_per_event,
                                        ArrayRef<float> current_times,
                                        EventStorage &event_storage,
                                        ArrayRef<Event *> events)
{
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
                                                ParticleTypeInfo &type_info,
                                                VectorAdaptor<uint> &r_unfinished_pindices)
{
  uint amount = step_data.array_size();
  TemporaryArray<int> next_event_indices(amount);
  TemporaryArray<float> time_factors_to_next_event(amount);
  TemporaryVector<uint> pindices_with_event;

  uint max_event_storage_size = std::max(get_max_event_storage_size(type_info.events), 1u);
  TemporaryArray<uint8_t> event_storage_array(max_event_storage_size * amount);
  EventStorage event_storage((void *)event_storage_array.begin(), max_event_storage_size);

  find_next_event_per_particle(step_data,
                               pindices,
                               type_info.events,
                               event_storage,
                               next_event_indices,
                               time_factors_to_next_event,
                               pindices_with_event);

  forward_particles_to_next_event_or_end(
      step_data, pindices, time_factors_to_next_event, type_info.offset_handlers);

  update_remaining_attribute_offsets(
      pindices_with_event, time_factors_to_next_event, step_data.attribute_offsets);

  update_remaining_durations(
      pindices_with_event, time_factors_to_next_event, step_data.remaining_durations);

  Vector<Vector<uint>> particles_per_event(type_info.events.size());
  find_pindices_per_event(pindices_with_event, next_event_indices, particles_per_event);

  TemporaryArray<float> current_times(amount);
  compute_current_time_per_particle(
      pindices_with_event, step_data.remaining_durations, step_data.step_end_time, current_times);

  execute_events(step_data, particles_per_event, current_times, event_storage, type_info.events);

  find_unfinished_particles(pindices_with_event,
                            time_factors_to_next_event,
                            step_data.attributes.get<uint8_t>("Kill State"),
                            r_unfinished_pindices);
}

BLI_NOINLINE static void simulate_with_max_n_events(BlockStepData &step_data,
                                                    uint max_events,
                                                    ParticleTypeInfo &type_info,
                                                    TemporaryVector<uint> &r_unfinished_pindices)
{
  TemporaryArray<uint> pindices_A(step_data.array_size());
  TemporaryArray<uint> pindices_B(step_data.array_size());

  uint amount_left = step_data.attributes.size();

  {
    /* Handle first event separately to be able to use the static number range. */
    VectorAdaptor<uint> pindices_output(pindices_A.begin(), amount_left);
    simulate_to_next_event(
        step_data, Range<uint>(0, amount_left).as_array_ref(), type_info, pindices_output);
    amount_left = pindices_output.size();
  }

  for (uint iteration = 0; iteration < max_events - 1 && amount_left > 0; iteration++) {
    VectorAdaptor<uint> pindices_input(pindices_A.begin(), amount_left, amount_left);
    VectorAdaptor<uint> pindices_output(pindices_B.begin(), amount_left, 0);

    simulate_to_next_event(step_data, pindices_input, type_info, pindices_output);
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

BLI_NOINLINE static void apply_remaining_offsets(BlockStepData &step_data,
                                                 ArrayRef<OffsetHandler *> offset_handlers,
                                                 ArrayRef<uint> pindices)
{
  if (offset_handlers.size() > 0) {
    TemporaryArray<float> time_factors(step_data.array_size());
    time_factors.fill_indices(pindices, 1.0f);

    OffsetHandlerInterface interface(step_data, pindices, time_factors);
    for (OffsetHandler *handler : offset_handlers) {
      handler->execute(interface);
    }
  }

  auto attribute_offsets = step_data.attribute_offsets;
  ParticleSet particles(step_data.attributes, pindices);

  for (uint attribute_index : attribute_offsets.info().attribute_indices()) {
    StringRef name = attribute_offsets.info().name_of(attribute_index);

    /* Only vectors can be integrated for now. */
    auto values = particles.attributes().get<float3>(name);
    auto offsets = attribute_offsets.get<float3>(attribute_index);

    if (particles.pindices_are_trivial()) {
      add_float3_arrays(values.take_front(particles.size()), offsets.take_front(particles.size()));
    }
    else {
      for (uint pindex : particles.pindices()) {
        values[pindex] += offsets[pindex];
      }
    }
  }
}

BLI_NOINLINE static void simulate_block(ParticleAllocator &particle_allocator,
                                        ParticlesBlock &block,
                                        ParticleTypeInfo &type_info,
                                        MutableArrayRef<float> remaining_durations,
                                        float end_time)
{
  uint amount = block.active_amount();
  BLI_assert(amount == remaining_durations.size());

  Integrator &integrator = *type_info.integrator;
  AttributesInfo &offsets_info = integrator.offset_attributes_info();
  Vector<void *> offset_buffers;
  for (AttributeType type : offsets_info.types()) {
    void *ptr = BLI_temporary_allocate(size_of_attribute_type(type) * amount);
    offset_buffers.append(ptr);
  }
  AttributeArrays attribute_offsets(offsets_info, offset_buffers, 0, amount);

  BlockStepData step_data = {
      particle_allocator, block.attributes(), attribute_offsets, remaining_durations, end_time};

  IntegratorInterface interface(step_data, block.active_range().as_array_ref());
  integrator.integrate(interface);

  if (type_info.events.size() == 0) {
    apply_remaining_offsets(
        step_data, type_info.offset_handlers, block.active_range().as_array_ref());
  }
  else {
    TemporaryVector<uint> unfinished_pindices;
    simulate_with_max_n_events(step_data, 10, type_info, unfinished_pindices);

    /* Not sure yet, if this really should be done. */
    if (unfinished_pindices.size() > 0) {
      apply_remaining_offsets(step_data, type_info.offset_handlers, unfinished_pindices);
    }
  }

  for (uint i = 0; i < offset_buffers.size(); i++) {
    BLI_temporary_deallocate(offset_buffers[i]);
  }
}

BLI_NOINLINE static void delete_tagged_particles_and_reorder(ParticlesBlock &block)
{
  auto kill_states = block.attributes().get<uint8_t>("Kill State");

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
  Vector<std::unique_ptr<ParticleAllocator>> m_allocators;

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

  Vector<ParticlesBlock *> gather_allocated_blocks()
  {
    Vector<ParticlesBlock *> blocks;
    for (auto &allocator : m_allocators) {
      blocks.extend(allocator->allocated_blocks());
    }
    return blocks;
  }
};

struct ThreadLocalData {
  ParticleAllocator &particle_allocator;

  ThreadLocalData(ParticleAllocator &particle_allocator) : particle_allocator(particle_allocator)
  {
  }
};

BLI_NOINLINE static void simulate_blocks_for_time_span(
    ParticleAllocators &block_allocators,
    ArrayRef<ParticlesBlock *> blocks,
    StringMap<ParticleTypeInfo> &types_to_simulate,
    TimeSpan time_span)
{
  if (blocks.size() == 0) {
    return;
  }

  BLI::Task::parallel_array_elements(
      blocks,
      /* Process individual element. */
      [&types_to_simulate, time_span](ParticlesBlock *block, ThreadLocalData *local_data) {
        ParticlesState &state = local_data->particle_allocator.particles_state();
        StringRef particle_type_name = state.particle_container_name(block->container());
        ParticleTypeInfo &type_info = types_to_simulate.lookup(particle_type_name);

        TemporaryArray<float> remaining_durations(block->active_amount());
        remaining_durations.fill(time_span.duration());

        simulate_block(local_data->particle_allocator,
                       *block,
                       type_info,
                       remaining_durations,
                       time_span.end());

        delete_tagged_particles_and_reorder(*block);
      },
      /* Create thread-local data. */
      [&block_allocators]() { return new ThreadLocalData(block_allocators.new_allocator()); },
      /* Free thread-local data. */
      [](ThreadLocalData *local_data) { delete local_data; },
      USE_THREADING);
}

BLI_NOINLINE static void simulate_blocks_from_birth_to_current_time(
    ParticleAllocators &block_allocators,
    ArrayRef<ParticlesBlock *> blocks,
    StringMap<ParticleTypeInfo> &types_to_simulate,
    float end_time)
{
  if (blocks.size() == 0) {
    return;
  }

  BLI::Task::parallel_array_elements(
      blocks,
      /* Process individual element. */
      [&types_to_simulate, end_time](ParticlesBlock *block, ThreadLocalData *local_data) {
        ParticlesState &state = local_data->particle_allocator.particles_state();
        StringRef particle_type_name = state.particle_container_name(block->container());
        ParticleTypeInfo &type_info = types_to_simulate.lookup(particle_type_name);

        uint active_amount = block->active_amount();
        Vector<float> durations(active_amount);
        auto birth_times = block->attributes().get<float>("Birth Time");
        for (uint i = 0; i < active_amount; i++) {
          durations[i] = end_time - birth_times[i];
        }
        simulate_block(local_data->particle_allocator, *block, type_info, durations, end_time);

        delete_tagged_particles_and_reorder(*block);
      },
      /* Create thread-local data. */
      [&block_allocators]() { return new ThreadLocalData(block_allocators.new_allocator()); },
      /* Free thread-local data. */
      [](ThreadLocalData *local_data) { delete local_data; },
      USE_THREADING);
}

BLI_NOINLINE static Vector<ParticlesBlock *> get_all_blocks_to_simulate(
    ParticlesState &state, StringMap<ParticleTypeInfo> &types_to_simulate)
{
  Vector<ParticlesBlock *> blocks;
  types_to_simulate.foreach_key([&state, &blocks](StringRefNull particle_type_name) {
    ParticlesContainer &container = state.particle_container(particle_type_name);
    blocks.extend(container.active_blocks());
  });
  return blocks;
}

BLI_NOINLINE static void compress_all_blocks(ParticlesContainer &container)
{
  Vector<ParticlesBlock *> blocks = container.active_blocks();
  ParticlesBlock::Compress(blocks);

  for (ParticlesBlock *block : blocks) {
    if (block->is_empty()) {
      container.release_block(*block);
    }
  }
}

BLI_NOINLINE static void compress_all_containers(ParticlesState &state)
{
  state.particle_containers().foreach_value(
      [](ParticlesContainer *container) { compress_all_blocks(*container); });
}

BLI_NOINLINE static void ensure_required_containers_exist(
    ParticlesState &state, StringMap<ParticleTypeInfo> &types_to_simulate)
{
  auto &containers = state.particle_containers();

  types_to_simulate.foreach_key([&containers](StringRefNull type_name) {
    if (!containers.contains(type_name)) {
      ParticlesContainer *container = new ParticlesContainer({}, 1000);
      containers.add_new(type_name, container);
    }
  });
}

BLI_NOINLINE static AttributesInfo build_attribute_info_for_type(ParticleTypeInfo &type_info,
                                                                 AttributesInfo &last_info)
{
  AttributesDeclaration builder;
  builder.join(last_info);
  builder.join(*type_info.attributes_declaration);

  for (Event *event : type_info.events) {
    event->attributes(builder);
  }

  builder.add<uint8_t>("Kill State", 0);
  builder.add<int32_t>("ID", 0);
  builder.add<float>("Birth Time", 0);

  return AttributesInfo(builder);
}

BLI_NOINLINE static void ensure_required_attributes_exist(
    ParticlesState &state, StringMap<ParticleTypeInfo> types_to_simulate)
{
  auto &containers = state.particle_containers();

  types_to_simulate.foreach_key_value_pair(
      [&containers](StringRefNull type_name, ParticleTypeInfo &type_info) {
        ParticlesContainer &container = *containers.lookup(type_name);

        AttributesInfo new_attributes_info = build_attribute_info_for_type(
            type_info, container.attributes_info());
        container.update_attributes(new_attributes_info);
      });
}

BLI_NOINLINE static void simulate_all_existing_blocks(
    ParticlesState &state,
    StringMap<ParticleTypeInfo> &types_to_simulate,
    ParticleAllocators &block_allocators,
    TimeSpan time_span)
{
  Vector<ParticlesBlock *> blocks = get_all_blocks_to_simulate(state, types_to_simulate);
  simulate_blocks_for_time_span(block_allocators, blocks, types_to_simulate, time_span);
}

BLI_NOINLINE static void create_particles_from_emitters(ParticleAllocators &block_allocators,
                                                        ArrayRef<Emitter *> emitters,
                                                        TimeSpan time_span,
                                                        WorldTransition &world_transition)
{
  ParticleAllocator &emitter_allocator = block_allocators.new_allocator();
  for (Emitter *emitter : emitters) {
    EmitterInterface interface(emitter_allocator, time_span, world_transition);
    emitter->emit(interface);
  }
}

BLI_NOINLINE static void emit_and_simulate_particles(
    ParticlesState &state,
    TimeSpan time_span,
    ArrayRef<Emitter *> emitters,
    StringMap<ParticleTypeInfo> &types_to_simulate,
    WorldTransition &world_transition)
{

  Vector<ParticlesBlock *> newly_created_blocks;
  {
    ParticleAllocators block_allocators(state);
    simulate_all_existing_blocks(state, types_to_simulate, block_allocators, time_span);
    create_particles_from_emitters(block_allocators, emitters, time_span, world_transition);
    newly_created_blocks = block_allocators.gather_allocated_blocks();
  }

  while (newly_created_blocks.size() > 0) {
    ParticleAllocators block_allocators(state);
    simulate_blocks_from_birth_to_current_time(
        block_allocators, newly_created_blocks, types_to_simulate, time_span.end());
    newly_created_blocks = block_allocators.gather_allocated_blocks();
  }
}

void simulate_particles(SimulationState &state,
                        WorldTransition &world_transition,
                        ArrayRef<Emitter *> emitters,
                        StringMap<ParticleTypeInfo> &types_to_simulate)
{
  SCOPED_TIMER(__func__);

  ParticlesState &particles_state = state.particles();

  ensure_required_containers_exist(particles_state, types_to_simulate);
  ensure_required_attributes_exist(particles_state, types_to_simulate);

  emit_and_simulate_particles(particles_state,
                              state.time().current_update_time(),
                              emitters,
                              types_to_simulate,
                              world_transition);

  compress_all_containers(particles_state);
}

}  // namespace BParticles
