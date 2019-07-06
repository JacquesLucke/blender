#pragma once

#include "FN_tuple_call.hpp"

#include "core.hpp"

namespace BParticles {

using FN::ExecutionContext;
using FN::SharedFunction;
using FN::Tuple;
using FN::TupleCallBody;

class Action {
 public:
  virtual ~Action() = 0;

  virtual void execute(EventExecuteInterface &interface) = 0;
};

class ParticleFunction;

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

  ParticleFunctionCaller get_caller(AttributeArrays attributes)
  {
    ParticleFunctionCaller caller;
    caller.m_body = m_tuple_call;

    for (uint i = 0; i < m_function->input_amount(); i++) {
      StringRef input_name = m_function->input_name(i);
      uint index = attributes.attribute_index(input_name);
      uint stride = attributes.attribute_stride(index);
      void *ptr = attributes.get_ptr(index);

      caller.m_attribute_buffers.append(ptr);
      caller.m_strides.append(stride);
    }

    return caller;
  }
};

Action *ACTION_none();
Action *ACTION_change_direction(ParticleFunction &compute_inputs, Action *post_action);
Action *ACTION_kill();
Action *ACTION_move(float3 offset);
Action *ACTION_spawn();
Action *ACTION_explode(StringRef new_particle_name,
                       ParticleFunction &compute_inputs,
                       Action *post_action);

}  // namespace BParticles
