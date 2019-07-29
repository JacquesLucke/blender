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

struct ParticleFunctionInputArray {
  void *buffer = nullptr;
  uint stride = 0;
  bool is_newly_allocated = false;

  ParticleFunctionInputArray(void *buffer, uint stride, bool is_newly_allocated)
      : buffer(buffer), stride(stride), is_newly_allocated(is_newly_allocated)
  {
  }

  template<typename T>
  ParticleFunctionInputArray(ArrayRef<T> array, bool is_newly_allocated)
      : ParticleFunctionInputArray((void *)array.begin(), sizeof(T), is_newly_allocated)
  {
  }
};

class InputProviderInterface {
 private:
  ArrayAllocator &m_array_allocator;
  ParticleSet m_particles;
  ActionContext *m_action_context;

 public:
  InputProviderInterface(ArrayAllocator &array_allocator,
                         ParticleSet particles,
                         ActionContext *action_context)
      : m_array_allocator(array_allocator),
        m_particles(particles),
        m_action_context(action_context)
  {
  }

  ArrayAllocator &array_allocator()
  {
    return m_array_allocator;
  }

  ParticleSet particles()
  {
    return m_particles;
  }

  ActionContext *action_context()
  {
    return m_action_context;
  }
};

class ParticleFunctionInputProvider {
 public:
  virtual ~ParticleFunctionInputProvider();

  virtual ParticleFunctionInputArray get(InputProviderInterface &interface) = 0;
};

class ParticleFunction {
 private:
  SharedFunction m_fn_no_deps;
  SharedFunction m_fn_with_deps;
  Vector<ParticleFunctionInputProvider *> m_input_providers;
  Vector<bool> m_parameter_depends_on_particle;
  Vector<uint> m_output_indices;

 public:
  ParticleFunction(SharedFunction fn_no_deps,
                   SharedFunction fn_with_deps,
                   Vector<ParticleFunctionInputProvider *> input_providers,
                   Vector<bool> parameter_depends_on_particle);

  ~ParticleFunction();

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
                                                  ParticleSet particles,
                                                  ActionContext *action_context);

  void init_without_deps(ParticleFunctionResult *result, ArrayAllocator &array_allocator);

  void init_with_deps(ParticleFunctionResult *result,
                      ArrayAllocator &array_allocator,
                      ParticleSet particles,
                      ActionContext *action_context);
};

}  // namespace BParticles
