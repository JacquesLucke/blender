#pragma once

#include "BLI_array_cxx.h"

#include "particle_allocator.hpp"
#include "emitter_interface.hpp"
#include "event_interface.hpp"
#include "offset_handler_interface.hpp"

namespace BParticles {

using BLI::LargeScopedArray;
using FN::AttributesRefGroup;

class ActionInterface {
 private:
  ParticleAllocator &m_particle_allocator;
  ArrayRef<uint> m_pindices;
  AttributesRef m_attributes;
  AttributesRef m_attribute_offsets;
  ArrayRef<float> m_current_times;
  ArrayRef<float> m_remaining_durations;

 public:
  ActionInterface(ParticleAllocator &particle_allocator,
                  ArrayRef<uint> pindices,
                  AttributesRef attributes,
                  AttributesRef attribute_offsets,
                  ArrayRef<float> current_times,
                  ArrayRef<float> remaining_durations);

  ArrayRef<uint> pindices();
  AttributesRef attributes();
  AttributesRef attribute_offsets();
  float remaining_time_in_step(uint pindex);
  ArrayRef<float> remaining_durations();
  ArrayRef<float> current_times();
  void kill(ArrayRef<uint> pindices);
  ParticleAllocator &particle_allocator();
};

class Action {
 public:
  virtual ~Action() = 0;

  virtual void execute(ActionInterface &interface) = 0;

  void execute_from_emitter(AttributesRefGroup &new_particles,
                            EmitterInterface &emitter_interface);
  void execute_from_event(EventExecuteInterface &event_interface);
  void execute_from_offset_handler(OffsetHandlerInterface &offset_handler_interface);
  void execute_for_subset(ArrayRef<uint> pindices, ActionInterface &action_interface);
  void execute_for_new_particles(AttributesRefGroup &new_particles,
                                 ActionInterface &action_interface);
  void execute_for_new_particles(AttributesRefGroup &new_particles,
                                 OffsetHandlerInterface &offset_handler_interface);
};

/* ActionInterface inline functions
 *******************************************/

inline ActionInterface::ActionInterface(ParticleAllocator &particle_allocator,
                                        ArrayRef<uint> pindices,
                                        AttributesRef attributes,
                                        AttributesRef attribute_offsets,
                                        ArrayRef<float> current_times,
                                        ArrayRef<float> remaining_durations)
    : m_particle_allocator(particle_allocator),
      m_pindices(pindices),
      m_attributes(attributes),
      m_attribute_offsets(attribute_offsets),
      m_current_times(current_times),
      m_remaining_durations(remaining_durations)
{
}

inline void Action::execute_from_emitter(AttributesRefGroup &new_particles,
                                         EmitterInterface &emitter_interface)
{
  AttributesInfo info;
  std::array<void *, 0> buffers;

  uint offset = 0;
  for (AttributesRef attributes : new_particles) {
    uint range_size = attributes.size();
    IndexRange range(offset, range_size);
    offset += range_size;

    AttributesRef offsets(info, buffers, range_size);
    LargeScopedArray<float> durations(range_size);
    durations.fill(0);

    ActionInterface action_interface(emitter_interface.particle_allocator(),
                                     IndexRange(range_size).as_array_ref(),
                                     attributes,
                                     offsets,
                                     attributes.get<float>("Birth Time"),
                                     durations);
    this->execute(action_interface);
  }
}

inline void Action::execute_from_event(EventExecuteInterface &event_interface)
{
  ActionInterface action_interface(event_interface.particle_allocator(),
                                   event_interface.pindices(),
                                   event_interface.attributes(),
                                   event_interface.attribute_offsets(),
                                   event_interface.current_times(),
                                   event_interface.remaining_durations());
  this->execute(action_interface);
}

inline void Action::execute_from_offset_handler(OffsetHandlerInterface &offset_handler_interface)
{
  LargeScopedArray<float> current_times(offset_handler_interface.array_size());
  for (uint pindex : offset_handler_interface.pindices()) {
    current_times[pindex] = offset_handler_interface.time_span(pindex).start();
  }

  ActionInterface action_interface(offset_handler_interface.particle_allocator(),
                                   offset_handler_interface.pindices(),
                                   offset_handler_interface.attributes(),
                                   offset_handler_interface.attribute_offsets(),
                                   current_times,
                                   offset_handler_interface.remaining_durations());
  this->execute(action_interface);
}

inline void Action::execute_for_subset(ArrayRef<uint> pindices, ActionInterface &action_interface)
{
  ActionInterface sub_interface(action_interface.particle_allocator(),
                                pindices,
                                action_interface.attributes(),
                                action_interface.attribute_offsets(),
                                action_interface.current_times(),
                                action_interface.remaining_durations());
  this->execute(sub_interface);
}

inline void Action::execute_for_new_particles(AttributesRefGroup &new_particles,
                                              ActionInterface &action_interface)
{
  AttributesInfo info;
  std::array<void *, 0> buffers;

  uint offset = 0;
  for (AttributesRef attributes : new_particles) {
    uint range_size = attributes.size();
    offset += range_size;

    AttributesRef offsets(info, buffers, range_size);
    LargeScopedArray<float> durations(range_size);
    durations.fill(0);

    ActionInterface new_interface(action_interface.particle_allocator(),
                                  IndexRange(range_size).as_array_ref(),
                                  attributes,
                                  offsets,
                                  attributes.get<float>("Birth Time"),
                                  durations);
    this->execute(new_interface);
  }
}

inline void Action::execute_for_new_particles(AttributesRefGroup &new_particles,
                                              OffsetHandlerInterface &offset_handler_interface)
{
  AttributesInfo info;
  std::array<void *, 0> buffers;

  for (AttributesRef attributes : new_particles) {
    uint range_size = attributes.size();

    AttributesRef offsets(info, buffers, range_size);
    LargeScopedArray<float> durations(range_size);
    durations.fill(0);

    ActionInterface new_interface(offset_handler_interface.particle_allocator(),
                                  IndexRange(range_size).as_array_ref(),
                                  attributes,
                                  offsets,
                                  attributes.get<float>("Birth Time"),
                                  durations);
    this->execute(new_interface);
  }
}

inline ArrayRef<uint> ActionInterface::pindices()
{
  return m_pindices;
}

inline AttributesRef ActionInterface::attributes()
{
  return m_attributes;
}

inline AttributesRef ActionInterface::attribute_offsets()
{
  return m_attribute_offsets;
}

inline float ActionInterface::remaining_time_in_step(uint pindex)
{
  return m_remaining_durations[pindex];
}

inline ArrayRef<float> ActionInterface::remaining_durations()
{
  return m_remaining_durations;
}

inline ArrayRef<float> ActionInterface::current_times()
{
  return m_current_times;
}

inline ParticleAllocator &ActionInterface::particle_allocator()
{
  return m_particle_allocator;
}

}  // namespace BParticles
