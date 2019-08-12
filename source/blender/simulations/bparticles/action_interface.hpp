#pragma once

#include "FN_tuple_call.hpp"

#include "step_description.hpp"

namespace BParticles {

using FN::ExecutionContext;
using FN::SharedFunction;
using FN::Tuple;
using FN::TupleCallBody;

class ActionContext {
 public:
  virtual ~ActionContext();
};

class ActionInterface {
 private:
  ParticleAllocator &m_particle_allocator;
  ArrayAllocator &m_array_allocator;
  ParticleSet m_particles;
  AttributeArrays m_attribute_offsets;
  ArrayRef<float> m_current_times;
  ArrayRef<float> m_remaining_durations;
  ActionContext &m_action_context;

 public:
  ActionInterface(ParticleAllocator &particle_allocator,
                  ArrayAllocator &array_allocator,
                  ParticleSet particles,
                  AttributeArrays attribute_offsets,
                  ArrayRef<float> current_times,
                  ArrayRef<float> remaining_durations,
                  ActionContext &action_context);

  ActionContext &context();

  ParticleSet &particles();
  AttributeArrays attribute_offsets();
  float remaining_time_in_step(uint pindex);
  ArrayRef<float> remaining_durations();
  ArrayRef<float> current_times();
  void kill(ArrayRef<uint> pindices);
  ParticleAllocator &particle_allocator();
  ArrayAllocator &array_allocator();
};

class Action {
 public:
  virtual ~Action() = 0;

  virtual void execute(ActionInterface &interface) = 0;

  void execute_from_emitter(ParticleSets &particle_sets,
                            EmitterInterface &emitter_interface,
                            ActionContext *action_context = nullptr);
  void execute_from_event(EventExecuteInterface &event_interface,
                          ActionContext *action_context = nullptr);
  void execute_for_subset(ArrayRef<uint> pindices, ActionInterface &action_interface);
  void execute_for_new_particles(ParticleSets &particle_sets, ActionInterface &action_interface);
};

/* ActionInterface inline functions
 *******************************************/

inline ActionInterface::ActionInterface(ParticleAllocator &particle_allocator,
                                        ArrayAllocator &array_allocator,
                                        ParticleSet particles,
                                        AttributeArrays attribute_offsets,
                                        ArrayRef<float> current_times,
                                        ArrayRef<float> remaining_durations,
                                        ActionContext &action_context)
    : m_particle_allocator(particle_allocator),
      m_array_allocator(array_allocator),
      m_particles(particles),
      m_attribute_offsets(attribute_offsets),
      m_current_times(current_times),
      m_remaining_durations(remaining_durations),
      m_action_context(action_context)
{
}

class EmptyEventInfo : public ActionContext {
};

inline void Action::execute_from_emitter(ParticleSets &particle_sets,
                                         EmitterInterface &emitter_interface,
                                         ActionContext *action_context)
{
  AttributesInfo info;
  std::array<void *, 0> buffers;

  EmptyEventInfo empty_action_context;
  ActionContext &used_action_context = (action_context == nullptr) ? empty_action_context :
                                                                     *action_context;

  for (ParticleSet particles : particle_sets.sets()) {
    AttributeArrays offsets(info, buffers, 0, particles.size());
    ArrayAllocator::Array<float> durations(emitter_interface.array_allocator());
    ArrayRef<float>(durations).fill_indices(particles.pindices(), 0);
    ActionInterface action_interface(emitter_interface.particle_allocator(),
                                     emitter_interface.array_allocator(),
                                     particles,
                                     offsets,
                                     particles.attributes().get<float>("Birth Time"),
                                     durations,
                                     used_action_context);
    this->execute(action_interface);
  }
}

inline void Action::execute_from_event(EventExecuteInterface &event_interface,
                                       ActionContext *action_context)
{
  EmptyEventInfo empty_action_context;
  ActionContext &used_action_context = (action_context == nullptr) ? empty_action_context :
                                                                     *action_context;

  ActionInterface action_interface(event_interface.particle_allocator(),
                                   event_interface.array_allocator(),
                                   event_interface.particles(),
                                   event_interface.attribute_offsets(),
                                   event_interface.current_times(),
                                   event_interface.remaining_durations(),
                                   used_action_context);
  this->execute(action_interface);
}

inline void Action::execute_for_subset(ArrayRef<uint> pindices, ActionInterface &action_interface)
{
  ActionInterface sub_interface(action_interface.particle_allocator(),
                                action_interface.array_allocator(),
                                ParticleSet(action_interface.particles().block(), pindices),
                                action_interface.attribute_offsets(),
                                action_interface.current_times(),
                                action_interface.remaining_durations(),
                                action_interface.context());
  this->execute(sub_interface);
}

inline void Action::execute_for_new_particles(ParticleSets &particle_sets,
                                              ActionInterface &action_interface)
{
  AttributesInfo info;
  std::array<void *, 0> buffers;

  /* Use empty action context, until there a better solution is implemented. */
  EmptyEventInfo empty_context;

  for (ParticleSet particles : particle_sets.sets()) {
    AttributeArrays offsets(info, buffers, 0, particles.size());
    ArrayAllocator::Array<float> durations(action_interface.array_allocator());
    ArrayRef<float>(durations).fill_indices(particles.pindices(), 0);
    ActionInterface new_interface(action_interface.particle_allocator(),
                                  action_interface.array_allocator(),
                                  particles,
                                  offsets,
                                  particles.attributes().get<float>("Birth Time"),
                                  durations,
                                  empty_context);
    this->execute(new_interface);
  }
}

inline ActionContext &ActionInterface::context()
{
  return m_action_context;
}

inline ParticleSet &ActionInterface::particles()
{
  return m_particles;
}

inline AttributeArrays ActionInterface::attribute_offsets()
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
  auto kill_states = m_particles.attributes().get<uint8_t>("Kill State");
  for (uint pindex : pindices) {
    kill_states[pindex] = 1;
  }
}

inline ParticleAllocator &ActionInterface::particle_allocator()
{
  return m_particle_allocator;
}

inline ArrayAllocator &ActionInterface::array_allocator()
{
  return m_array_allocator;
}

}  // namespace BParticles
