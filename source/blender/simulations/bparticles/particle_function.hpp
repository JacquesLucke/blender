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
using FN::GenericVectorArray;
using FN::MultiFunction;

class ParticleFunction {
 private:
  const MultiFunction &m_fn;
  Vector<std::string> m_computed_names;
  Vector<uint> m_index_mapping;

  const BKE::IDDataCache &m_id_data_cache;
  const BKE::IDHandleLookup &m_id_handle_lookup;

  friend class ParticleFunctionResult;

 public:
  ParticleFunction(const MultiFunction &fn,
                   Vector<std::string> computed_names,
                   const BKE::IDDataCache &id_data_cache,
                   const BKE::IDHandleLookup &id_handle_lookup);

  const MultiFunction &fn() const
  {
    return m_fn;
  }
};

class ParticleFunctionResult : BLI::NonCopyable {
 private:
  Vector<GenericVectorArray *> m_vector_arrays;
  Vector<GenericMutableArrayRef> m_arrays;

  ArrayRef<uint> m_index_mapping;
  ArrayRef<uint> m_indices;
  ArrayRef<std::string> m_computed_names;

  ParticleFunctionResult() = default;

 public:
  ~ParticleFunctionResult();
  ParticleFunctionResult(ParticleFunctionResult &&other) = default;

  static ParticleFunctionResult Compute(const ParticleFunction &particle_fn,
                                        ArrayRef<uint> indices,
                                        AttributesRef attributes);

  const void *get_single(StringRef expected_name, uint param_index, uint pindex)
  {
    UNUSED_VARS_NDEBUG(expected_name);
#ifdef DEBUG
    StringRef actual_name = m_computed_names[param_index];
    BLI_assert(expected_name == actual_name);
#endif
    uint corrected_index = m_index_mapping[param_index];
    return m_arrays[corrected_index][pindex];
  }

  template<typename T> const T &get_single(StringRef expected_name, uint param_index, uint pindex)
  {
    UNUSED_VARS_NDEBUG(expected_name);
#ifdef DEBUG
    StringRef actual_name = m_computed_names[param_index];
    BLI_assert(expected_name == actual_name);
#endif
    uint corrected_index = m_index_mapping[param_index];
    ArrayRef<T> array = m_arrays[corrected_index].as_typed_ref<T>();
    return array[pindex];
  }

  template<typename T>
  ArrayRef<T> get_vector(StringRef expected_name, uint param_index, uint pindex)
  {
    UNUSED_VARS_NDEBUG(expected_name);
#ifdef DEBUG
    StringRef actual_name = m_computed_names[param_index];
    BLI_assert(expected_name == actual_name);
#endif
    uint corrected_index = m_index_mapping[param_index];
    GenericVectorArray &vector_array = *m_vector_arrays[corrected_index];
    return vector_array[pindex].as_typed_ref<T>();
  }

  GenericVectorArray &computed_vector_array(uint param_index)
  {
    uint corrected_index = m_index_mapping[param_index];
    return *m_vector_arrays[corrected_index];
  }

  GenericArrayRef computed_array(uint param_index)
  {
    uint corrected_index = m_index_mapping[param_index];
    return m_arrays[corrected_index];
  }
};

}  // namespace BParticles
