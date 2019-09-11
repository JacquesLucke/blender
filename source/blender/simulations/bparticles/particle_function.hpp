#pragma once

#include "FN_tuple_call.hpp"
#include "FN_functions.hpp"
#include "BLI_array.hpp"

#include "action_interface.hpp"
#include "force_interface.hpp"

namespace BParticles {

using BLI::ArrayRef;
using BLI::Optional;
using BLI::TemporaryArray;
using BLI::TemporaryVector;
using BLI::Vector;
using FN::CPPTypeInfo;
using FN::ExecutionContext;
using FN::ExecutionStack;
using FN::SharedFunction;
using FN::TupleCallBody;
using FN::Type;

class ParticleFunction;

class ParticleFunctionResult : BLI::NonCopyable, BLI::NonMovable {
 private:
  Vector<void *> m_buffers;
  Vector<uint> m_strides;
  Vector<bool> m_only_first;
  FN::Function *m_fn_no_deps;
  FN::Function *m_fn_with_deps;
  ArrayRef<uint> m_output_indices;
  ArrayRef<uint> m_pindices;

  friend ParticleFunction;

 public:
  ParticleFunctionResult() = default;

  ~ParticleFunctionResult()
  {
    /* Free elements in buffers. */
    for (uint i = 0; i < m_only_first.size(); i++) {
      uint output_index = m_output_indices[i];
      if (m_only_first[i]) {
        auto &type_info = m_fn_no_deps->output_type(output_index)->extension<CPPTypeInfo>();
        void *ptr = m_buffers[i];
        type_info.destruct(ptr);
      }
      else {
        auto &type_info = m_fn_with_deps->output_type(output_index)->extension<CPPTypeInfo>();
        if (!type_info.trivially_destructible()) {
          void *ptr = m_buffers[i];
          for (uint pindex : m_pindices) {
            type_info.destruct(POINTER_OFFSET(ptr, type_info.size() * pindex));
          }
        }
      }
    }

    /* Free buffers. */
    for (uint i = 0; i < m_buffers.size(); i++) {
      BLI_temporary_deallocate(m_buffers[i]);
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

struct ParticleTimes {
 public:
  enum Type {
    Current,
    DurationAndEnd,
  };

 private:
  Type m_type;
  ArrayRef<float> m_current_times;
  ArrayRef<float> m_remaining_durations;
  float m_end_time;

  ParticleTimes(Type type,
                ArrayRef<float> current_times,
                ArrayRef<float> remaining_durations,
                float end_time)
      : m_type(type),
        m_current_times(current_times),
        m_remaining_durations(remaining_durations),
        m_end_time(end_time)
  {
  }

 public:
  static ParticleTimes FromCurrentTimes(ArrayRef<float> current_times)
  {
    return ParticleTimes(Type::Current, current_times, {}, 0);
  }

  static ParticleTimes FromDurationsAndEnd(ArrayRef<float> remaining_durations, float end_time)
  {
    return ParticleTimes(Type::DurationAndEnd, {}, remaining_durations, end_time);
  }

  Type type()
  {
    return m_type;
  }

  ArrayRef<float> current_times()
  {
    BLI_assert(m_type == Type::Current);
    return m_current_times;
  }

  ArrayRef<float> remaining_durations()
  {
    BLI_assert(m_type == Type::DurationAndEnd);
    return m_remaining_durations;
  }

  float end_time()
  {
    BLI_assert(m_type == Type::DurationAndEnd);
    return m_end_time;
  }
};

class InputProviderInterface {
 private:
  ArrayRef<uint> m_pindices;
  AttributesRef m_attributes;
  ParticleTimes m_particle_times;
  ActionContext *m_action_context;

 public:
  InputProviderInterface(ArrayRef<uint> pindices,
                         AttributesRef attributes,
                         ParticleTimes particle_times,
                         ActionContext *action_context)
      : m_pindices(pindices),
        m_attributes(attributes),
        m_particle_times(particle_times),
        m_action_context(action_context)
  {
  }

  ArrayRef<uint> pindices()
  {
    return m_pindices;
  }

  AttributesRef attributes()
  {
    return m_attributes;
  }

  ParticleTimes &particle_times()
  {
    return m_particle_times;
  }

  ActionContext *action_context()
  {
    return m_action_context;
  }
};

class ParticleFunctionInputProvider {
 public:
  virtual ~ParticleFunctionInputProvider();

  virtual Optional<ParticleFunctionInputArray> get(InputProviderInterface &interface) = 0;
};

class ParticleFunction {
 private:
  SharedFunction m_fn_no_deps;
  SharedFunction m_fn_with_deps;
  Vector<ParticleFunctionInputProvider *> m_input_providers;
  Vector<bool> m_parameter_depends_on_particle;
  Vector<uint> m_output_indices;
  std::unique_ptr<FN::Functions::ArrayExecution> m_array_execution;

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

  std::unique_ptr<ParticleFunctionResult> compute(ArrayRef<uint> pindices,
                                                  AttributesRef attributes,
                                                  ParticleTimes particle_times,
                                                  ActionContext *action_context);

 private:
  void init_without_deps(ParticleFunctionResult *result);

  void init_with_deps(ParticleFunctionResult *result,
                      ArrayRef<uint> pindices,
                      AttributesRef attributes,
                      ParticleTimes particle_times,
                      ActionContext *action_context);
};

}  // namespace BParticles
