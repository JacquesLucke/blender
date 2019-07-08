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

  ParticleFunctionCaller get_caller(AttributeArrays attributes, EventInfo &event_info)
  {
    ParticleFunctionCaller caller;
    caller.m_body = m_tuple_call;

    for (uint i = 0; i < m_function->input_amount(); i++) {
      StringRef input_name = m_function->input_name(i);
      void *ptr = nullptr;
      uint stride = 0;
      if (input_name.startswith("Event")) {
        StringRef event_attribute_name = input_name.drop_prefix("Event: ");
        ptr = event_info.get_info_array(event_attribute_name);
        stride = sizeof(float3); /* TODO make not hardcoded */
      }
      else if (input_name.startswith("Attribute")) {
        StringRef attribute_name = input_name.drop_prefix("Attribute: ");
        uint index = attributes.attribute_index(attribute_name);
        stride = attributes.attribute_stride(index);
        ptr = attributes.get_ptr(index);
      }
      else {
        BLI_assert(false);
      }

      BLI_assert(ptr);
      caller.m_attribute_buffers.append(ptr);
      caller.m_strides.append(stride);
    }

    return caller;
  }
};

class ActionInterface {
 private:
  EventExecuteInterface &m_event_execute_interface;
  EventInfo &m_event_info;

 public:
  ActionInterface(EventExecuteInterface &event_execute_interface, EventInfo &event_info)
      : m_event_execute_interface(event_execute_interface), m_event_info(event_info)
  {
  }

  EventExecuteInterface &execute_interface()
  {
    return m_event_execute_interface;
  }

  EventInfo &event_info()
  {
    return m_event_info;
  }

  ParticleSet &particles()
  {
    return m_event_execute_interface.particles();
  }

  AttributeArrays attribute_offsets()
  {
    return m_event_execute_interface.attribute_offsets();
  }

  float remaining_time_in_step(uint index)
  {
    return m_event_execute_interface.step_end_time() -
           m_event_execute_interface.current_times()[index];
  }

  ArrayRef<float> current_times()
  {
    return m_event_execute_interface.current_times();
  }

  void kill(ArrayRef<uint> particle_indices)
  {
    m_event_execute_interface.kill(particle_indices);
  }

  InstantEmitTarget &request_emit_target(StringRef particle_type_name,
                                         ArrayRef<uint> original_indices)
  {
    return m_event_execute_interface.request_emit_target(particle_type_name, original_indices);
  }

  void execute_action_for_subset(ArrayRef<uint> indices, std::unique_ptr<Action> &action);
};

class Action {
 public:
  virtual ~Action() = 0;

  virtual void execute(ActionInterface &interface) = 0;
};

Action *ACTION_none();
Action *ACTION_change_direction(ParticleFunction &compute_inputs, Action *post_action);
Action *ACTION_kill();
Action *ACTION_explode(StringRef new_particle_name,
                       ParticleFunction &compute_inputs,
                       Action *post_action);
Action *ACTION_condition(ParticleFunction &compute_inputs,
                         Action *true_action,
                         Action *false_action);

}  // namespace BParticles
