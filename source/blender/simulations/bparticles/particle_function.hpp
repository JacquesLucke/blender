#pragma once

#include "BLI_array_cxx.h"

#include "action_interface.hpp"
#include "force_interface.hpp"

#include "FN_multi_function.h"
#include "FN_multi_function_common_contexts.h"

#include "BKE_id_data_cache.h"

namespace BParticles {

using BLI::ArrayRef;
using BLI::LargeScopedArray;
using BLI::Optional;
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

  void *get(StringRef UNUSED(expected_name), uint parameter_index, uint pindex)
  {
    return m_computed_buffers[parameter_index][pindex];
  }
};

class ParticleFunction {
 private:
  const MultiFunction &m_fn;
  const BKE::IDDataCache &m_id_data_cache;
  const BKE::IDHandleLookup &m_id_handle_lookup;

 public:
  ParticleFunction(const MultiFunction &fn,
                   const BKE::IDDataCache &id_data_cache,
                   const BKE::IDHandleLookup &id_handle_lookup);

  ~ParticleFunction();

  std::unique_ptr<ParticleFunctionResult> compute(ActionInterface &interface);
  std::unique_ptr<ParticleFunctionResult> compute(OffsetHandlerInterface &interface);
  std::unique_ptr<ParticleFunctionResult> compute(ForceInterface &interface);
  std::unique_ptr<ParticleFunctionResult> compute(EventFilterInterface &interface);

  std::unique_ptr<ParticleFunctionResult> compute(ArrayRef<uint> pindices,
                                                  AttributesRef attributes);
};

}  // namespace BParticles
