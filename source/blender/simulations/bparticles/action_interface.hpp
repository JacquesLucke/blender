#pragma once

#include "FN_tuple_call.hpp"

#include "core.hpp"

namespace BParticles {

using FN::ExecutionContext;
using FN::SharedFunction;
using FN::Tuple;
using FN::TupleCallBody;

class EventInfo {
 public:
  virtual void *get_info_array(StringRef name) = 0;
};

class ParticleFunction;
class Action;

class ParticleFunctionCaller {
 private:
  TupleCallBody *m_body;
  SmallVector<void *> m_attribute_buffers;
  SmallVector<uint> m_strides;

  friend ParticleFunction;

 public:
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx, uint pindex)
  {
    BLI_assert(fn_in.size() == m_attribute_buffers.size());

    for (uint i = 0; i < fn_in.size(); i++) {
      void *ptr = POINTER_OFFSET(m_attribute_buffers[i], pindex * m_strides[i]);
      fn_in.copy_in__dynamic(i, ptr);
    }

    m_body->call(fn_in, fn_out, ctx);
  }

  TupleCallBody *body() const
  {
    return m_body;
  }
};

class ParticleFunction {
 private:
  SharedFunction m_function;
  TupleCallBody *m_tuple_call;

 public:
  ParticleFunction(SharedFunction &fn) : m_function(fn)
  {
    m_tuple_call = fn->body<TupleCallBody>();
    BLI_assert(m_tuple_call);
  }

  ParticleFunctionCaller get_caller(AttributeArrays attributes, EventInfo &event_info);
};

class ActionInterface {
 private:
  ParticleAllocator &m_particle_allocator;
  ArrayAllocator &m_array_allocator;
  ParticleSet m_particles;
  AttributeArrays m_attribute_offsets;
  EventInfo &m_event_info;
  ArrayRef<float> m_current_times;
  float m_step_end_time;

 public:
  ActionInterface(ParticleAllocator &particle_allocator,
                  ArrayAllocator &array_allocator,
                  ParticleSet particles,
                  AttributeArrays attribute_offsets,
                  ArrayRef<float> current_times,
                  float step_end_time,
                  EventInfo &event_info);

  EventInfo &event_info();

  ParticleSet &particles();
  AttributeArrays attribute_offsets();
  float remaining_time_in_step(uint index);
  ArrayRef<float> current_times();
  void kill(ArrayRef<uint> particle_indices);
  void execute_action_for_subset(ArrayRef<uint> indices, std::unique_ptr<Action> &action);
  ParticleAllocator &particle_allocator();
  ArrayAllocator &array_allocator();
};

class Action {
 public:
  virtual ~Action() = 0;

  virtual void execute(ActionInterface &interface) = 0;
};

/* ActionInterface inline functions
 *******************************************/

inline ActionInterface::ActionInterface(ParticleAllocator &particle_allocator,
                                        ArrayAllocator &array_allocator,
                                        ParticleSet particles,
                                        AttributeArrays attribute_offsets,
                                        ArrayRef<float> current_times,
                                        float step_end_time,
                                        EventInfo &event_info)
    : m_particle_allocator(particle_allocator),
      m_array_allocator(array_allocator),
      m_particles(particles),
      m_attribute_offsets(attribute_offsets),
      m_current_times(current_times),
      m_step_end_time(step_end_time),
      m_event_info(event_info)
{
}

inline EventInfo &ActionInterface::event_info()
{
  return m_event_info;
}

inline ParticleSet &ActionInterface::particles()
{
  return m_particles;
}

inline AttributeArrays ActionInterface::attribute_offsets()
{
  return m_attribute_offsets;
}

inline float ActionInterface::remaining_time_in_step(uint index)
{
  return m_step_end_time - m_current_times[index];
}

inline ArrayRef<float> ActionInterface::current_times()
{
  return m_current_times;
}

inline void ActionInterface::kill(ArrayRef<uint> particle_indices)
{
  auto kill_states = m_particles.attributes().get_byte("Kill State");
  for (uint pindex : particle_indices) {
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
