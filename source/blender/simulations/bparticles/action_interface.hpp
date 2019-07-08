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
  EventExecuteInterface &m_event_execute_interface;
  EventInfo &m_event_info;

 public:
  ActionInterface(EventExecuteInterface &event_execute_interface, EventInfo &event_info);

  EventInfo &event_info();

  ParticleSet &particles();
  AttributeArrays attribute_offsets();
  float remaining_time_in_step(uint index);
  ArrayRef<float> current_times();
  void kill(ArrayRef<uint> particle_indices);
  InstantEmitTarget &request_emit_target(StringRef particle_type_name,
                                         ArrayRef<uint> original_indices);
  void execute_action_for_subset(ArrayRef<uint> indices, std::unique_ptr<Action> &action);
};

class Action {
 public:
  virtual ~Action() = 0;

  virtual void execute(ActionInterface &interface) = 0;
};

/* ActionInterface inline functions
 *******************************************/

inline ActionInterface::ActionInterface(EventExecuteInterface &event_execute_interface,
                                        EventInfo &event_info)
    : m_event_execute_interface(event_execute_interface), m_event_info(event_info)
{
}

inline EventInfo &ActionInterface::event_info()
{
  return m_event_info;
}

inline ParticleSet &ActionInterface::particles()
{
  return m_event_execute_interface.particles();
}

inline AttributeArrays ActionInterface::attribute_offsets()
{
  return m_event_execute_interface.attribute_offsets();
}

inline float ActionInterface::remaining_time_in_step(uint index)
{
  return m_event_execute_interface.step_end_time() -
         m_event_execute_interface.current_times()[index];
}

inline ArrayRef<float> ActionInterface::current_times()
{
  return m_event_execute_interface.current_times();
}

inline void ActionInterface::kill(ArrayRef<uint> particle_indices)
{
  m_event_execute_interface.kill(particle_indices);
}

inline InstantEmitTarget &ActionInterface::request_emit_target(StringRef particle_type_name,
                                                               ArrayRef<uint> original_indices)
{
  return m_event_execute_interface.request_emit_target(particle_type_name, original_indices);
}

}  // namespace BParticles
