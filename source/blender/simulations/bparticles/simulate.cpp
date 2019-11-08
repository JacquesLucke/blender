
#include "BLI_lazy_init_cxx.h"
#include "BLI_task_cxx.h"
#include "BLI_timeit.h"
#include "BLI_array_cxx.h"
#include "BLI_vector_adaptor.h"

#include "FN_cpp_type.h"

#include "simulate.hpp"
#include "time_span.hpp"

#define USE_THREADING true

namespace BParticles {

using BLI::TemporaryArray;
using BLI::TemporaryVector;
using BLI::VectorAdaptor;
using FN::CPPType;

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
    ParticleAllocator &particle_allocator,
    ArrayRef<uint> pindices,
    ArrayRef<float> time_factors_to_next_event,
    ArrayRef<OffsetHandler *> offset_handlers)
{
  OffsetHandlerInterface interface(
      step_data, pindices, time_factors_to_next_event, particle_allocator);
  for (OffsetHandler *handler : offset_handlers) {
    handler->execute(interface);
  }

  auto attributes = step_data.attributes;
  auto attribute_offsets = step_data.attribute_offsets;
  for (uint attribute_index : attribute_offsets.info().indices()) {
    StringRef name = attribute_offsets.info().name_of(attribute_index);

    /* Only vectors can be integrated for now. */
    auto values = attributes.get<float3>(name);
    auto offsets = attribute_offsets.get<float3>(attribute_index);

    for (uint pindex : pindices) {
      float time_factor = time_factors_to_next_event[pindex];
      values[pindex] += time_factor * offsets[pindex];
    }
  }
}

BLI_NOINLINE static void update_remaining_attribute_offsets(
    ArrayRef<uint> pindices_with_event,
    ArrayRef<float> time_factors_to_next_event,
    AttributesRef attribute_offsets)
{
  for (uint attribute_index : attribute_offsets.info().indices()) {
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
                                        ParticleAllocator &particle_allocator,
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

    EventExecuteInterface interface(
        step_data, pindices, current_times, event_storage, particle_allocator);
    event->execute(interface);
  }
}

BLI_NOINLINE static void simulate_to_next_event(BlockStepData &step_data,
                                                ParticleAllocator &particle_allocator,
                                                ArrayRef<uint> pindices,
                                                ParticleSystemInfo &system_info,
                                                VectorAdaptor<uint> &r_unfinished_pindices)
{
  uint amount = step_data.array_size();
  TemporaryArray<int> next_event_indices(amount);
  TemporaryArray<float> time_factors_to_next_event(amount);
  TemporaryVector<uint> pindices_with_event;

  uint max_event_storage_size = std::max(get_max_event_storage_size(system_info.events), 1u);
  TemporaryArray<uint8_t> event_storage_array(max_event_storage_size * amount);
  EventStorage event_storage((void *)event_storage_array.begin(), max_event_storage_size);

  find_next_event_per_particle(step_data,
                               pindices,
                               system_info.events,
                               event_storage,
                               next_event_indices,
                               time_factors_to_next_event,
                               pindices_with_event);

  forward_particles_to_next_event_or_end(step_data,
                                         particle_allocator,
                                         pindices,
                                         time_factors_to_next_event,
                                         system_info.offset_handlers);

  update_remaining_attribute_offsets(
      pindices_with_event, time_factors_to_next_event, step_data.attribute_offsets);

  update_remaining_durations(
      pindices_with_event, time_factors_to_next_event, step_data.remaining_durations);

  Vector<Vector<uint>> particles_per_event(system_info.events.size());
  find_pindices_per_event(pindices_with_event, next_event_indices, particles_per_event);

  TemporaryArray<float> current_times(amount);
  compute_current_time_per_particle(
      pindices_with_event, step_data.remaining_durations, step_data.step_end_time, current_times);

  execute_events(step_data,
                 particle_allocator,
                 particles_per_event,
                 current_times,
                 event_storage,
                 system_info.events);

  find_unfinished_particles(pindices_with_event,
                            time_factors_to_next_event,
                            step_data.attributes.get<uint8_t>("Kill State"),
                            r_unfinished_pindices);
}

BLI_NOINLINE static void simulate_with_max_n_events(BlockStepData &step_data,
                                                    ParticleAllocator &particle_allocator,
                                                    uint max_events,
                                                    ParticleSystemInfo &system_info,
                                                    TemporaryVector<uint> &r_unfinished_pindices)
{
  TemporaryArray<uint> pindices_A(step_data.array_size());
  TemporaryArray<uint> pindices_B(step_data.array_size());

  uint amount_left = step_data.attributes.size();

  {
    /* Handle first event separately to be able to use the static number range. */
    VectorAdaptor<uint> pindices_output(pindices_A.begin(), amount_left);
    simulate_to_next_event(step_data,
                           particle_allocator,
                           IndexRange(amount_left).as_array_ref(),
                           system_info,
                           pindices_output);
    amount_left = pindices_output.size();
  }

  for (uint iteration = 0; iteration < max_events - 1 && amount_left > 0; iteration++) {
    VectorAdaptor<uint> pindices_input(pindices_A.begin(), amount_left, amount_left);
    VectorAdaptor<uint> pindices_output(pindices_B.begin(), amount_left, 0);

    simulate_to_next_event(
        step_data, particle_allocator, pindices_input, system_info, pindices_output);
    amount_left = pindices_output.size();
    std::swap(pindices_A, pindices_B);
  }

  for (uint i = 0; i < amount_left; i++) {
    r_unfinished_pindices.append(pindices_A[i]);
  }
}

BLI_NOINLINE static void apply_remaining_offsets(BlockStepData &step_data,
                                                 ParticleAllocator &particle_allocator,
                                                 ArrayRef<OffsetHandler *> offset_handlers,
                                                 ArrayRef<uint> pindices)
{
  if (offset_handlers.size() > 0) {
    TemporaryArray<float> time_factors(step_data.array_size());
    time_factors.fill_indices(pindices, 1.0f);

    OffsetHandlerInterface interface(step_data, pindices, time_factors, particle_allocator);
    for (OffsetHandler *handler : offset_handlers) {
      handler->execute(interface);
    }
  }

  auto attributes = step_data.attributes;
  auto attribute_offsets = step_data.attribute_offsets;

  for (uint attribute_index : attribute_offsets.info().indices()) {
    StringRef name = attribute_offsets.info().name_of(attribute_index);

    /* Only vectors can be integrated for now. */
    auto values = attributes.get<float3>(name);
    auto offsets = attribute_offsets.get<float3>(attribute_index);

    for (uint pindex : pindices) {
      values[pindex] += offsets[pindex];
    }
  }
}

BLI_NOINLINE static void simulate_block(SimulationState &simulation_state,
                                        ParticleAllocator &particle_allocator,
                                        AttributesBlock &block,
                                        ParticleSystemInfo &system_info,
                                        MutableArrayRef<float> remaining_durations,
                                        float end_time)
{
  uint amount = block.used_size();
  BLI_assert(amount == remaining_durations.size());

  Integrator &integrator = *system_info.integrator;
  const AttributesInfo &offsets_info = integrator.offset_attributes_info();
  Vector<void *> offset_buffers;
  for (const CPPType *type : offsets_info.types()) {
    void *ptr = BLI_temporary_allocate(type->size() * amount);
    offset_buffers.append(ptr);
  }
  AttributesRef attribute_offsets(offsets_info, offset_buffers, amount);

  BlockStepData step_data = {
      simulation_state, block.as_ref(), attribute_offsets, remaining_durations, end_time};

  IntegratorInterface interface(step_data, block.used_range().as_array_ref());
  integrator.integrate(interface);

  if (system_info.events.size() == 0) {
    apply_remaining_offsets(step_data,
                            particle_allocator,
                            system_info.offset_handlers,
                            block.used_range().as_array_ref());
  }
  else {
    TemporaryVector<uint> unfinished_pindices;
    simulate_with_max_n_events(
        step_data, particle_allocator, 10, system_info, unfinished_pindices);

    /* Not sure yet, if this really should be done. */
    if (unfinished_pindices.size() > 0) {
      apply_remaining_offsets(
          step_data, particle_allocator, system_info.offset_handlers, unfinished_pindices);
    }
  }

  for (uint i = 0; i < offset_buffers.size(); i++) {
    BLI_temporary_deallocate(offset_buffers[i]);
  }
}

BLI_NOINLINE static void delete_tagged_particles_and_reorder(AttributesBlock &block)
{
  auto kill_states = block.as_ref().get<uint8_t>("Kill State");
  TemporaryVector<uint> indices_to_delete;

  for (uint i = 0; i < kill_states.size(); i++) {
    if (kill_states[i]) {
      indices_to_delete.append(i);
    }
  }

  block.destruct_and_reorder(indices_to_delete);
}

BLI_NOINLINE static void simulate_blocks_for_time_span(
    ParticleAllocator &particle_allocator,
    ArrayRef<AttributesBlock *> blocks,
    StringMap<ParticleSystemInfo> &systems_to_simulate,
    TimeSpan time_span,
    SimulationState &simulation_state)
{
  if (blocks.size() == 0) {
    return;
  }

  BLI::Task::parallel_array_elements(
      blocks,
      /* Process individual element. */
      [&](AttributesBlock *block) {
        StringRef particle_system_name = simulation_state.particles().particle_container_name(
            block->owner());
        ParticleSystemInfo &system_info = systems_to_simulate.lookup(particle_system_name);

        TemporaryArray<float> remaining_durations(block->used_size());
        remaining_durations.fill(time_span.duration());

        simulate_block(simulation_state,
                       particle_allocator,
                       *block,
                       system_info,
                       remaining_durations,
                       time_span.end());

        delete_tagged_particles_and_reorder(*block);
      },
      USE_THREADING);
}

BLI_NOINLINE static void simulate_blocks_from_birth_to_current_time(
    ParticleAllocator &particle_allocator,
    ArrayRef<AttributesBlock *> blocks,
    StringMap<ParticleSystemInfo> &systems_to_simulate,
    float end_time,
    SimulationState &simulation_state)
{
  if (blocks.size() == 0) {
    return;
  }

  BLI::Task::parallel_array_elements(
      blocks,
      /* Process individual element. */
      [&](AttributesBlock *block) {
        StringRef particle_system_name = simulation_state.particles().particle_container_name(
            block->owner());
        ParticleSystemInfo &system_info = systems_to_simulate.lookup(particle_system_name);

        uint active_amount = block->used_size();
        Vector<float> durations(active_amount);
        auto birth_times = block->as_ref().get<float>("Birth Time");
        for (uint i = 0; i < active_amount; i++) {
          durations[i] = end_time - birth_times[i];
        }
        simulate_block(
            simulation_state, particle_allocator, *block, system_info, durations, end_time);

        delete_tagged_particles_and_reorder(*block);
      },
      USE_THREADING);
}

BLI_NOINLINE static Vector<AttributesBlock *> get_all_blocks_to_simulate(
    ParticlesState &state, StringMap<ParticleSystemInfo> &systems_to_simulate)
{
  Vector<AttributesBlock *> blocks;
  systems_to_simulate.foreach_key([&state, &blocks](StringRefNull particle_system_name) {
    AttributesBlockContainer &container = state.particle_container(particle_system_name);
    blocks.extend(container.active_blocks());
  });
  return blocks;
}

BLI_NOINLINE static void compress_all_blocks(AttributesBlockContainer &container)
{
  Vector<AttributesBlock *> blocks = container.active_blocks();
  AttributesBlock::Compress(blocks);

  for (AttributesBlock *block : blocks) {
    if (block->used_size() == 0) {
      container.release_block(*block);
    }
  }
}

BLI_NOINLINE static void compress_all_containers(ParticlesState &state)
{
  state.particle_containers().foreach_value(
      [](AttributesBlockContainer *container) { compress_all_blocks(*container); });
}

BLI_NOINLINE static void simulate_all_existing_blocks(
    SimulationState &simulation_state,
    StringMap<ParticleSystemInfo> &systems_to_simulate,
    ParticleAllocator &particle_allocator,
    TimeSpan time_span)
{
  Vector<AttributesBlock *> blocks = get_all_blocks_to_simulate(simulation_state.particles(),
                                                                systems_to_simulate);
  simulate_blocks_for_time_span(
      particle_allocator, blocks, systems_to_simulate, time_span, simulation_state);
}

BLI_NOINLINE static void create_particles_from_emitters(SimulationState &simulation_state,
                                                        ParticleAllocator &particle_allocator,
                                                        ArrayRef<Emitter *> emitters,
                                                        TimeSpan time_span)
{
  for (Emitter *emitter : emitters) {
    EmitterInterface interface(simulation_state, particle_allocator, time_span);
    emitter->emit(interface);
  }
}

void simulate_particles(SimulationState &simulation_state,
                        ArrayRef<Emitter *> emitters,
                        StringMap<ParticleSystemInfo> &systems_to_simulate)
{
  SCOPED_TIMER(__func__);

  ParticlesState &particles_state = simulation_state.particles();
  TimeSpan simulation_time_span = simulation_state.time().current_update_time();

  Vector<AttributesBlock *> newly_created_blocks;
  {
    ParticleAllocator particle_allocator(particles_state);
    simulate_all_existing_blocks(
        simulation_state, systems_to_simulate, particle_allocator, simulation_time_span);
    create_particles_from_emitters(
        simulation_state, particle_allocator, emitters, simulation_time_span);
    newly_created_blocks = particle_allocator.allocated_blocks();
  }

  while (newly_created_blocks.size() > 0) {
    ParticleAllocator particle_allocator(particles_state);
    simulate_blocks_from_birth_to_current_time(particle_allocator,
                                               newly_created_blocks,
                                               systems_to_simulate,
                                               simulation_time_span.end(),
                                               simulation_state);
    newly_created_blocks = particle_allocator.allocated_blocks();
  }

  compress_all_containers(particles_state);
}

}  // namespace BParticles
