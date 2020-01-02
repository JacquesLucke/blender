
#include "BLI_lazy_init_cxx.h"
#include "BLI_timeit.h"
#include "BLI_array_cxx.h"
#include "BLI_vector_adaptor.h"
#include "BLI_parallel.h"

#include "FN_cpp_type.h"

#include "simulate.hpp"

namespace BParticles {

using BLI::LargeScopedArray;
using BLI::LargeScopedVector;
using BLI::VectorAdaptor;
using FN::CPPType;

BLI_NOINLINE static void find_next_event_per_particle(
    BlockStepData &step_data,
    IndexMask mask,
    ArrayRef<Event *> events,
    MutableArrayRef<int> r_next_event_indices,
    MutableArrayRef<float> r_time_factors_to_next_event,
    LargeScopedVector<uint> &r_pindices_with_event)
{
  r_next_event_indices.fill_indices(mask, -1);
  r_time_factors_to_next_event.fill_indices(mask, 1.0f);

  for (uint event_index : events.index_range()) {
    Vector<uint> triggered_pindices;
    Vector<float> triggered_time_factors;

    Event *event = events[event_index];
    EventFilterInterface interface(
        step_data, mask, r_time_factors_to_next_event, triggered_pindices, triggered_time_factors);
    event->filter(interface);

    for (uint i : triggered_pindices.index_range()) {
      uint pindex = triggered_pindices[i];
      float time_factor = triggered_time_factors[i];
      BLI_assert(time_factor <= r_time_factors_to_next_event[pindex]);

      r_next_event_indices[pindex] = event_index;
      r_time_factors_to_next_event[pindex] = time_factor;
    }
  }

  for (uint pindex : mask) {
    if (r_next_event_indices[pindex] != -1) {
      r_pindices_with_event.append(pindex);
    }
  }
}

BLI_NOINLINE static void forward_particles_to_next_event_or_end(
    BlockStepData &step_data,
    ParticleAllocator &particle_allocator,
    IndexMask mask,
    ArrayRef<float> time_factors_to_next_event,
    ArrayRef<OffsetHandler *> offset_handlers)
{
  OffsetHandlerInterface interface(
      step_data, mask, time_factors_to_next_event, particle_allocator);
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

    for (uint pindex : mask) {
      float time_factor = time_factors_to_next_event[pindex];
      values[pindex] += time_factor * offsets[pindex];
    }
  }
}

BLI_NOINLINE static void update_remaining_attribute_offsets(
    IndexMask mask,
    ArrayRef<float> time_factors_to_next_event,
    MutableAttributesRef attribute_offsets)
{
  for (uint attribute_index : attribute_offsets.info().indices()) {
    /* Only vectors can be integrated for now. */
    auto offsets = attribute_offsets.get<float3>(attribute_index);

    for (uint pindex : mask) {
      float factor = 1.0f - time_factors_to_next_event[pindex];
      offsets[pindex] *= factor;
    }
  }
}

BLI_NOINLINE static void update_remaining_durations(IndexMask mask,
                                                    ArrayRef<float> time_factors_to_next_event,
                                                    MutableArrayRef<float> remaining_durations)
{
  for (uint pindex : mask) {
    remaining_durations[pindex] *= (1.0f - time_factors_to_next_event[pindex]);
  }
}

BLI_NOINLINE static void find_pindices_per_event(
    IndexMask mask,
    ArrayRef<int> next_event_indices,
    MutableArrayRef<Vector<uint>> r_particles_per_event)
{
  for (uint pindex : mask) {
    int event_index = next_event_indices[pindex];
    BLI_assert(event_index >= 0);
    r_particles_per_event[event_index].append(pindex);
  }
}

BLI_NOINLINE static void compute_current_time_per_particle(IndexMask mask,
                                                           ArrayRef<float> remaining_durations,
                                                           float end_time,
                                                           MutableArrayRef<float> r_current_times)
{
  for (uint pindex : mask) {
    r_current_times[pindex] = end_time - remaining_durations[pindex];
  }
}

BLI_NOINLINE static void find_unfinished_particles(IndexMask mask,
                                                   ArrayRef<float> time_factors_to_next_event,
                                                   ArrayRef<bool> kill_states,
                                                   VectorAdaptor<uint> &r_unfinished_pindices)
{
  for (uint pindex : mask) {
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
                                        ArrayRef<Event *> events)
{
  BLI_assert(events.size() == pindices_per_event.size());

  for (uint event_index : events.index_range()) {
    Event *event = events[event_index];
    ArrayRef<uint> pindices = pindices_per_event[event_index];

    if (pindices.size() == 0) {
      continue;
    }

    EventExecuteInterface interface(step_data, pindices, current_times, particle_allocator);
    event->execute(interface);
  }
}

BLI_NOINLINE static void simulate_to_next_event(BlockStepData &step_data,
                                                ParticleAllocator &particle_allocator,
                                                IndexMask mask,
                                                ParticleSystemInfo &system_info,
                                                VectorAdaptor<uint> &r_unfinished_pindices)
{
  uint amount = step_data.array_size();
  LargeScopedArray<int> next_event_indices(amount);
  LargeScopedArray<float> time_factors_to_next_event(amount);
  LargeScopedVector<uint> pindices_with_event;

  find_next_event_per_particle(step_data,
                               mask,
                               system_info.events,
                               next_event_indices,
                               time_factors_to_next_event,
                               pindices_with_event);

  forward_particles_to_next_event_or_end(step_data,
                                         particle_allocator,
                                         mask,
                                         time_factors_to_next_event,
                                         system_info.offset_handlers);

  update_remaining_attribute_offsets(
      pindices_with_event, time_factors_to_next_event, step_data.attribute_offsets);

  update_remaining_durations(
      pindices_with_event, time_factors_to_next_event, step_data.remaining_durations);

  Vector<Vector<uint>> particles_per_event(system_info.events.size());
  find_pindices_per_event(pindices_with_event, next_event_indices, particles_per_event);

  LargeScopedArray<float> current_times(amount);
  compute_current_time_per_particle(
      pindices_with_event, step_data.remaining_durations, step_data.step_end_time, current_times);

  execute_events(
      step_data, particle_allocator, particles_per_event, current_times, system_info.events);

  find_unfinished_particles(pindices_with_event,
                            time_factors_to_next_event,
                            step_data.attributes.get<bool>("Dead"),
                            r_unfinished_pindices);
}

BLI_NOINLINE static void simulate_with_max_n_events(BlockStepData &step_data,
                                                    ParticleAllocator &particle_allocator,
                                                    uint max_events,
                                                    ParticleSystemInfo &system_info,
                                                    LargeScopedVector<uint> &r_unfinished_pindices)
{
  LargeScopedArray<uint> pindices_A(step_data.array_size());
  LargeScopedArray<uint> pindices_B(step_data.array_size());

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
                                                 IndexMask mask)
{
  if (offset_handlers.size() > 0) {
    LargeScopedArray<float> time_factors(step_data.array_size());
    time_factors.fill_indices(mask, 1.0f);

    OffsetHandlerInterface interface(step_data, mask, time_factors, particle_allocator);
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

    for (uint pindex : mask) {
      values[pindex] += offsets[pindex];
    }
  }
}

BLI_NOINLINE static void simulate_particle_chunk(SimulationState &simulation_state,
                                                 ParticleAllocator &particle_allocator,
                                                 MutableAttributesRef attributes,
                                                 ParticleSystemInfo &system_info,
                                                 MutableArrayRef<float> remaining_durations,
                                                 float end_time)
{
  uint amount = attributes.size();
  BLI_assert(amount == remaining_durations.size());

  Integrator &integrator = *system_info.integrator;
  const AttributesInfo &offsets_info = integrator.offset_attributes_info();
  Vector<void *> offset_buffers;
  for (const CPPType *type : offsets_info.types()) {
    void *ptr = BLI_temporary_allocate(type->size() * amount);
    offset_buffers.append(ptr);
  }
  MutableAttributesRef attribute_offsets(offsets_info, offset_buffers, amount);

  BlockStepData step_data = {
      simulation_state, attributes, attribute_offsets, remaining_durations, end_time};

  IntegratorInterface interface(step_data, IndexRange(amount).as_array_ref());
  integrator.integrate(interface);

  if (system_info.events.size() == 0) {
    apply_remaining_offsets(step_data,
                            particle_allocator,
                            system_info.offset_handlers,
                            IndexRange(amount).as_array_ref());
  }
  else {
    LargeScopedVector<uint> unfinished_pindices;
    simulate_with_max_n_events(
        step_data, particle_allocator, 10, system_info, unfinished_pindices);

    /* Not sure yet, if this really should be done. */
    if (unfinished_pindices.size() > 0) {
      apply_remaining_offsets(
          step_data, particle_allocator, system_info.offset_handlers, unfinished_pindices);
    }
  }

  for (void *buffer : offset_buffers) {
    BLI_temporary_deallocate(buffer);
  }
}

BLI_NOINLINE static void delete_tagged_particles_and_reorder(ParticleSet &particles)
{
  auto kill_states = particles.attributes().get<bool>("Dead");
  LargeScopedVector<uint> indices_to_delete;

  for (uint i : kill_states.index_range()) {
    if (kill_states[i]) {
      indices_to_delete.append(i);
    }
  }

  particles.destruct_and_reorder(indices_to_delete.as_ref());
}

BLI_NOINLINE static void simulate_particles_for_time_span(SimulationState &simulation_state,
                                                          ParticleAllocator &particle_allocator,
                                                          ParticleSystemInfo &system_info,
                                                          FloatInterval time_span,
                                                          MutableAttributesRef particle_attributes)
{
  BLI::blocked_parallel_for(IndexRange(particle_attributes.size()), 1000, [&](IndexRange range) {
    Array<float> remaining_durations(range.size(), time_span.size());
    simulate_particle_chunk(simulation_state,
                            particle_allocator,
                            particle_attributes.slice(range),
                            system_info,
                            remaining_durations,
                            time_span.end());
  });
}

BLI_NOINLINE static void simulate_particles_from_birth_to_end_of_step(
    SimulationState &simulation_state,
    ParticleAllocator &particle_allocator,
    ParticleSystemInfo &system_info,
    float end_time,
    MutableAttributesRef particle_attributes)
{
  ArrayRef<float> all_birth_times = particle_attributes.get<float>("Birth Time");

  BLI::blocked_parallel_for(IndexRange(particle_attributes.size()), 1000, [&](IndexRange range) {
    ArrayRef<float> birth_times = all_birth_times.slice(range);

    Array<float> remaining_durations(range.size());
    for (uint i : remaining_durations.index_range()) {
      remaining_durations[i] = end_time - birth_times[i];
    }

    simulate_particle_chunk(simulation_state,
                            particle_allocator,
                            particle_attributes.slice(range),
                            system_info,
                            remaining_durations,
                            end_time);
  });
}

BLI_NOINLINE static void simulate_existing_particles(
    SimulationState &simulation_state,
    ParticleAllocator &particle_allocator,
    StringMap<ParticleSystemInfo> &systems_to_simulate)
{
  FloatInterval simulation_time_span = simulation_state.time().current_update_time();

  BLI::parallel_map_items(simulation_state.particles().particle_containers(),
                          [&](StringRef system_name, ParticleSet *particle_set) {
                            ParticleSystemInfo *system_info = systems_to_simulate.lookup_ptr(
                                system_name);
                            if (system_info == nullptr) {
                              return;
                            }

                            simulate_particles_for_time_span(simulation_state,
                                                             particle_allocator,
                                                             *system_info,
                                                             simulation_time_span,
                                                             particle_set->attributes());
                          });
}

BLI_NOINLINE static void create_particles_from_emitters(SimulationState &simulation_state,
                                                        ParticleAllocator &particle_allocator,
                                                        ArrayRef<Emitter *> emitters,
                                                        FloatInterval time_span)
{
  BLI::parallel_for(emitters.index_range(), [&](uint emitter_index) {
    Emitter &emitter = *emitters[emitter_index];
    EmitterInterface interface(simulation_state, particle_allocator, time_span);
    emitter.emit(interface);
  });
}

void simulate_particles(SimulationState &simulation_state,
                        ArrayRef<Emitter *> emitters,
                        StringMap<ParticleSystemInfo> &systems_to_simulate)
{
  SCOPED_TIMER(__func__);

  ParticlesState &particles_state = simulation_state.particles();
  FloatInterval simulation_time_span = simulation_state.time().current_update_time();

  StringMultiMap<ParticleSet *> all_newly_created_particles;
  StringMultiMap<ParticleSet *> newly_created_particles;
  {
    ParticleAllocator particle_allocator(particles_state);
    BLI::parallel_invoke(
        [&]() {
          simulate_existing_particles(simulation_state, particle_allocator, systems_to_simulate);
        },
        [&]() {
          create_particles_from_emitters(
              simulation_state, particle_allocator, emitters, simulation_time_span);
        });

    newly_created_particles = particle_allocator.allocated_particles();
    all_newly_created_particles = newly_created_particles;
  }

  while (newly_created_particles.key_amount() > 0) {
    ParticleAllocator particle_allocator(particles_state);

    BLI::parallel_map_items(
        newly_created_particles, [&](StringRef name, ArrayRef<ParticleSet *> new_particle_sets) {
          ParticleSystemInfo *system_info = systems_to_simulate.lookup_ptr(name);
          if (system_info == nullptr) {
            return;
          }

          BLI::parallel_for(new_particle_sets.index_range(), [&](uint index) {
            ParticleSet &particle_set = *new_particle_sets[index];
            simulate_particles_from_birth_to_end_of_step(simulation_state,
                                                         particle_allocator,
                                                         *system_info,
                                                         simulation_time_span.end(),
                                                         particle_set.attributes());
          });
        });

    newly_created_particles = particle_allocator.allocated_particles();
    all_newly_created_particles.add_multiple(newly_created_particles);
  }

  BLI::parallel_map_items(all_newly_created_particles,
                          [&](StringRef name, ArrayRef<ParticleSet *> new_particle_sets) {
                            ParticleSet &main_set = particles_state.particle_container(name);

                            for (ParticleSet *set : new_particle_sets) {
                              main_set.add_particles(*set);
                              delete set;
                            }

                            delete_tagged_particles_and_reorder(main_set);
                          });
}

}  // namespace BParticles
