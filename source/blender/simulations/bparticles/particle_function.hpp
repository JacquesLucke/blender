#pragma once

#include "BLI_array_cxx.h"

#include "action_interface.hpp"
#include "force_interface.hpp"

#include "FN_multi_function.h"
#include "FN_multi_function_common_contexts.h"

namespace BParticles {

using BLI::ArrayRef;
using BLI::Optional;
using BLI::TemporaryArray;
using BLI::TemporaryVector;
using BLI::Vector;
using FN::GenericArrayRef;
using FN::GenericMutableArrayRef;
using FN::MultiFunction;

class ParticleFunction;

class ParticleFunctionResult : BLI::NonCopyable, BLI::NonMovable {
 private:
  Vector<GenericMutableArrayRef> m_computed_buffers;
  ArrayRef<uint> m_pindices;

  friend ParticleFunction;

 public:
  ParticleFunctionResult() = default;

  ~ParticleFunctionResult()
  {
    for (GenericMutableArrayRef array : m_computed_buffers) {
      array.destruct_indices(m_pindices);
      BLI_temporary_deallocate(array.buffer());
    }
  }

  template<typename T> T get(StringRef UNUSED(expected_name), uint parameter_index, uint pindex)
  {
    return m_computed_buffers[parameter_index].as_typed_ref<T>()[pindex];
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
  ArrayRef<uint> m_pindices;
  AttributesRef m_attributes;
  ActionContext *m_action_context;

 public:
  InputProviderInterface(ArrayRef<uint> pindices,
                         AttributesRef attributes,
                         ActionContext *action_context)
      : m_pindices(pindices), m_attributes(attributes), m_action_context(action_context)
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
  std::unique_ptr<const MultiFunction> m_fn;
  Vector<ParticleFunctionInputProvider *> m_input_providers;
  FN::ExternalDataCacheContext &m_data_cache;
  FN::PersistentSurfacesLookupContext &m_persistent_surface_lookup;

 public:
  ParticleFunction(std::unique_ptr<const MultiFunction> fn,
                   Vector<ParticleFunctionInputProvider *> input_providers,
                   FN::ExternalDataCacheContext &data_cache,
                   FN::PersistentSurfacesLookupContext &persistent_surface_lookup);

  ~ParticleFunction();

  std::unique_ptr<ParticleFunctionResult> compute(ActionInterface &interface);
  std::unique_ptr<ParticleFunctionResult> compute(OffsetHandlerInterface &interface);
  std::unique_ptr<ParticleFunctionResult> compute(ForceInterface &interface);
  std::unique_ptr<ParticleFunctionResult> compute(EventFilterInterface &interface);

  std::unique_ptr<ParticleFunctionResult> compute(ArrayRef<uint> pindices,
                                                  AttributesRef attributes,
                                                  ActionContext *action_context);
};

}  // namespace BParticles
