#pragma once

#include "FN_tuple_call.hpp"
#include "BLI_array.hpp"

#include "particle_allocator.hpp"
#include "emitter_interface.hpp"
#include "event_interface.hpp"
#include "offset_handler_interface.hpp"

namespace BParticles {

using BLI::TemporaryArray;
using BLI::TemporaryVector;
using FN::ExecutionContext;
using FN::SharedFunction;
using FN::Tuple;
using FN::TupleCallBody;

class ActionContext {
 public:
  virtual ~ActionContext();
};

class EmitterActionContext {
 public:
  virtual void update(Range<uint> slice) = 0;
};

class ActionInterface {
 private:
  ParticleAllocator &m_particle_allocator;
  ArrayRef<uint> m_pindices;
  AttributesRef m_attributes;
  AttributesRef m_attribute_offsets;
  ArrayRef<float> m_current_times;
  ArrayRef<float> m_remaining_durations;
  ActionContext &m_action_context;

 public:
  ActionInterface(ParticleAllocator &particle_allocator,
                  ArrayRef<uint> pindices,
                  AttributesRef attributes,
                  AttributesRef attribute_offsets,
                  ArrayRef<float> current_times,
                  ArrayRef<float> remaining_durations,
                  ActionContext &action_context);

  ActionContext &context();

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
                            EmitterInterface &emitter_interface,
                            EmitterActionContext *emitter_action_context = nullptr);
  void execute_from_event(EventExecuteInterface &event_interface,
                          ActionContext *action_context = nullptr);
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
                                        ArrayRef<float> remaining_durations,
                                        ActionContext &action_context)
    : m_particle_allocator(particle_allocator),
      m_pindices(pindices),
      m_attributes(attributes),
      m_attribute_offsets(attribute_offsets),
      m_current_times(current_times),
      m_remaining_durations(remaining_durations),
      m_action_context(action_context)
{
}

class EmptyActionContext : public ActionContext {
};

class EmptyEmitterActionContext : public EmitterActionContext {
  void update(Range<uint> UNUSED(slice))
  {
  }
};

inline void Action::execute_from_emitter(AttributesRefGroup &new_particles,
                                         EmitterInterface &emitter_interface,
                                         EmitterActionContext *emitter_action_context)
{
  AttributesInfo info;
  std::array<void *, 0> buffers;

  EmptyEmitterActionContext empty_emitter_context;
  EmitterActionContext &used_emitter_context = (emitter_action_context == nullptr) ?
                                                   empty_emitter_context :
                                                   *emitter_action_context;

  uint offset = 0;
  for (uint i = 0; i < new_particles.range_amount(); i++) {
    AttributesRef attributes = new_particles.segment(i);
    uint range_size = attributes.size();

    used_emitter_context.update(Range<uint>(offset, offset + range_size));
    offset += range_size;

    AttributesRef offsets(info, buffers, range_size);
    TemporaryArray<float> durations(range_size);
    durations.fill(0);

    ActionInterface action_interface(emitter_interface.particle_allocator(),
                                     Range<uint>(0, range_size).as_array_ref(),
                                     attributes,
                                     offsets,
                                     attributes.get<float>("Birth Time"),
                                     durations,
                                     dynamic_cast<ActionContext &>(used_emitter_context));
    this->execute(action_interface);
  }
}

inline void Action::execute_from_event(EventExecuteInterface &event_interface,
                                       ActionContext *action_context)
{
  EmptyActionContext empty_action_context;
  ActionContext &used_action_context = (action_context == nullptr) ? empty_action_context :
                                                                     *action_context;

  ActionInterface action_interface(event_interface.particle_allocator(),
                                   event_interface.pindices(),
                                   event_interface.attributes(),
                                   event_interface.attribute_offsets(),
                                   event_interface.current_times(),
                                   event_interface.remaining_durations(),
                                   used_action_context);
  this->execute(action_interface);
}

inline void Action::execute_for_subset(ArrayRef<uint> pindices, ActionInterface &action_interface)
{
  ActionInterface sub_interface(action_interface.particle_allocator(),
                                pindices,
                                action_interface.attributes(),
                                action_interface.attribute_offsets(),
                                action_interface.current_times(),
                                action_interface.remaining_durations(),
                                action_interface.context());
  this->execute(sub_interface);
}

inline void Action::execute_for_new_particles(AttributesRefGroup &new_particles,
                                              ActionInterface &action_interface)
{
  AttributesInfo info;
  std::array<void *, 0> buffers;

  /* Use empty action context, until there a better solution is implemented. */
  EmptyActionContext empty_context;

  for (uint i = 0; i < new_particles.range_amount(); i++) {
    AttributesRef attributes = new_particles.segment(i);
    uint range_size = attributes.size();

    AttributesRef offsets(info, buffers, range_size);
    TemporaryArray<float> durations(range_size);
    durations.fill(0);

    ActionInterface new_interface(action_interface.particle_allocator(),
                                  Range<uint>(0, range_size).as_array_ref(),
                                  attributes,
                                  offsets,
                                  attributes.get<float>("Birth Time"),
                                  durations,
                                  empty_context);
    this->execute(new_interface);
  }
}

inline void Action::execute_for_new_particles(AttributesRefGroup &new_particles,
                                              OffsetHandlerInterface &offset_handler_interface)
{
  AttributesInfo info;
  std::array<void *, 0> buffers;

  EmptyActionContext empty_context;

  for (uint i = 0; i < new_particles.range_amount(); i++) {
    AttributesRef attributes = new_particles.segment(i);
    uint range_size = attributes.size();

    AttributesRef offsets(info, buffers, range_size);
    TemporaryArray<float> durations(range_size);
    durations.fill(0);

    ActionInterface new_interface(offset_handler_interface.particle_allocator(),
                                  Range<uint>(0, range_size).as_array_ref(),
                                  attributes,
                                  offsets,
                                  attributes.get<float>("Birth Time"),
                                  durations,
                                  empty_context);
    this->execute(new_interface);
  }
}

inline ActionContext &ActionInterface::context()
{
  return m_action_context;
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

inline void ActionInterface::kill(ArrayRef<uint> pindices)
{
  auto kill_states = m_attributes.get<uint8_t>("Kill State");
  for (uint pindex : pindices) {
    kill_states[pindex] = 1;
  }
}

inline ParticleAllocator &ActionInterface::particle_allocator()
{
  return m_particle_allocator;
}

}  // namespace BParticles
