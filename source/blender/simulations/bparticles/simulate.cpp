
#include "BLI_timeit.h"
#include "BLI_array_cxx.h"
#include "BLI_vector_adaptor.h"
#include "BLI_parallel.h"

#include "FN_cpp_type.h"

#include "simulate.hpp"

namespace BParticles {

using BLI::ScopedVector;
using BLI::VectorAdaptor;
using FN::CPPType;

BLI_NOINLINE static void simulate_particle_chunk(SimulationState &UNUSED(simulation_state),
                                                 ParticleAllocator &UNUSED(particle_allocator),
                                                 MutableAttributesRef attributes,
                                                 ParticleSystemInfo &system_info,
                                                 MutableArrayRef<float> remaining_durations,
                                                 float UNUSED(end_time))
{
  uint amount = attributes.size();
  BLI_assert(amount == remaining_durations.size());

  BufferCache buffer_cache;

  Array<float3> forces(attributes.size(), {0, 0, 0});
  for (Force *force : system_info.forces) {
    force->add_force(attributes, IndexRange(amount), buffer_cache, forces);
  }

  MutableArrayRef<float3> velocities = attributes.get<float3>("Velocity");
  MutableArrayRef<float3> positions = attributes.get<float3>("Position");

  for (uint pindex : IndexRange(amount)) {
    float mass = 1.0f;
    float duration = remaining_durations[pindex];

    velocities[pindex] += duration * forces[pindex] / mass;
    positions[pindex] += duration * velocities[pindex];
  }
}

BLI_NOINLINE static void delete_tagged_particles_and_reorder(ParticleSet &particles)
{
  auto kill_states = particles.attributes().get<bool>("Dead");
  ScopedVector<uint> indices_to_delete;

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

  systems_to_simulate.foreach_item([](StringRef name, ParticleSystemInfo &system_info) {
    system_info.collision_objects.print_as_lines(
        name, [](Object *object) { std::cout << object->id.name; });
  });

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
                          });

  BLI::parallel_map_keys(systems_to_simulate, [&](StringRef name) {
    ParticleSet &particles = particles_state.particle_container(name);
    delete_tagged_particles_and_reorder(particles);
  });
}

}  // namespace BParticles
