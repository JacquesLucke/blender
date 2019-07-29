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
  FN::Function *m_fn_no_deps;
  FN::Function *m_fn_with_deps;
  ArrayRef<uint> m_output_indices;

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
    uint output_index = m_output_indices[parameter_index];
    if (m_only_first[parameter_index]) {
      StringRefNull real_name = m_fn_no_deps->output_name(output_index);
      BLI_assert(real_name == expected_name);
    }
    else {
      StringRefNull real_name = m_fn_with_deps->output_name(output_index);
      BLI_assert(real_name == expected_name);
    }
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
  SharedFunction m_fn_no_deps;
  SharedFunction m_fn_with_deps;
  Vector<bool> m_parameter_depends_on_particle;
  Vector<uint> m_output_indices;

 public:
  ParticleFunction(SharedFunction fn_no_deps,
                   SharedFunction fn_with_deps,
                   Vector<bool> parameter_depends_on_particle)
      : m_fn_no_deps(std::move(fn_no_deps)),
        m_fn_with_deps(std::move(fn_with_deps)),
        m_parameter_depends_on_particle(std::move(parameter_depends_on_particle))
  {
    BLI_assert(m_fn_no_deps->output_amount() + m_fn_with_deps->output_amount() ==
               m_parameter_depends_on_particle.size());
    BLI_assert(m_fn_no_deps->input_amount() == 0);

    uint no_deps_index = 0;
    uint with_deps_index = 0;
    for (uint i = 0; i < m_parameter_depends_on_particle.size(); i++) {
      if (m_parameter_depends_on_particle[i]) {
        m_output_indices.append(with_deps_index);
        with_deps_index++;
      }
      else {
        m_output_indices.append(no_deps_index);
        no_deps_index++;
      }
    }
  }

  SharedFunction &function_no_deps()
  {
    return m_fn_no_deps;
  }

  bool parameter_depends_on_particle(StringRef expected_name, uint parameter_index)
  {
    bool depends_on_particle = m_parameter_depends_on_particle[parameter_index];
#ifdef DEBUG
    uint output_index = m_output_indices[parameter_index];
    if (depends_on_particle) {
      StringRefNull real_name = m_fn_with_deps->output_name(output_index);
      BLI_assert(expected_name == real_name);
    }
    else {
      StringRefNull real_name = m_fn_no_deps->output_name(output_index);
      BLI_assert(expected_name == real_name);
    }
#endif
    UNUSED_VARS_NDEBUG(expected_name);
    return depends_on_particle;
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
