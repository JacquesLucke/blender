#pragma once

#include "FN_tuple_call.hpp"
#include "attributes.hpp"
#include "action_interface.hpp"
#include "force_interface.hpp"

namespace BParticles {

using BLI::ArrayRef;
using BLI::Vector;
using FN::CPPTypeInfo;
using FN::ExecutionContext;
using FN::ExecutionStack;
using FN::SharedFunction;
using FN::SharedType;
using FN::TupleCallBody;

class ParticleFunction;

class ParticleFunctionResult {
 private:
  Vector<void *> m_buffers;
  Vector<uint> m_strides;
  Vector<bool> m_only_first;
  ArrayAllocator *m_array_allocator;
  FN::Function *m_fn;

  friend ParticleFunction;

 public:
  ParticleFunctionResult() = default;
  ParticleFunctionResult(ParticleFunctionResult &other) = delete;
  ParticleFunctionResult(ParticleFunctionResult &&other) = delete;

  ~ParticleFunctionResult()
  {
    for (uint i = 0; i < m_buffers.size(); i++) {
      m_array_allocator->deallocate(m_buffers[i], m_strides[i]);
    }
  }

  template<typename T> T get(StringRef expected_name, uint parameter_index, uint pindex)
  {
#ifdef DEBUG
    BLI_assert(sizeof(T) == m_strides[parameter_index]);
    StringRefNull real_name = m_fn->output_name(parameter_index);
    BLI_assert(real_name == expected_name);
#endif
    UNUSED_VARS_NDEBUG(expected_name);

    T *buffer = (T *)m_buffers[parameter_index];
    if (m_only_first[parameter_index]) {
      return buffer[0];
    }
    else {
      return buffer[pindex];
    }
  }
};

class ParticleFunction {
 private:
  SharedFunction m_fn;
  TupleCallBody *m_body;

 public:
  ParticleFunction(SharedFunction fn) : m_fn(std::move(fn)), m_body(&m_fn->body<TupleCallBody>())
  {
    BLI_assert(m_body != nullptr);
  }

  bool depends_on_particle()
  {
    return m_fn->input_amount() > 0;
  }

  SharedFunction &function()
  {
    return m_fn;
  }

  std::unique_ptr<ParticleFunctionResult> compute(ActionInterface &interface);
  std::unique_ptr<ParticleFunctionResult> compute(OffsetHandlerInterface &interface);
  std::unique_ptr<ParticleFunctionResult> compute(ForceInterface &interface);
  std::unique_ptr<ParticleFunctionResult> compute(EventFilterInterface &interface);

 private:
  std::unique_ptr<ParticleFunctionResult> compute(ArrayAllocator &array_allocator,
                                                  ArrayRef<uint> pindices,
                                                  AttributeArrays attributes,
                                                  ActionContext *action_context);
};

}  // namespace BParticles
