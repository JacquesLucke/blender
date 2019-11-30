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

class ParticleFunction {
 private:
  const MultiFunction &m_fn;
  FN::ExternalDataCacheContext &m_data_cache;
  FN::PersistentSurfacesLookupContext &m_persistent_surface_lookup;

 public:
  ParticleFunction(const MultiFunction &fn,
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
