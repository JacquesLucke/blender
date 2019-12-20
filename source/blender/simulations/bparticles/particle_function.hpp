#pragma once

#include "BLI_array_cxx.h"

#include "particle_action.hpp"
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

  friend class ParticleFunctionEvaluator;

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

class ParticleFunctionEvaluator {
 private:
  const ParticleFunction &m_particle_fn;
  IndexMask m_mask;
  AttributesRef m_particle_attributes;
  bool m_is_computed = false;

  FN::MFContextBuilder m_context_builder;

  Vector<GenericVectorArray *> m_computed_vector_arrays;
  Vector<GenericMutableArrayRef> m_computed_arrays;

 public:
  ParticleFunctionEvaluator(const ParticleFunction &particle_fn,
                            IndexMask mask,
                            AttributesRef particle_attributes)
      : m_particle_fn(particle_fn), m_mask(mask), m_particle_attributes(particle_attributes)
  {
  }

  ~ParticleFunctionEvaluator();

  FN::MFContextBuilder &context_builder()
  {
    return m_context_builder;
  }

  void compute();

  /* Access computed values
   *********************************************/

  const void *get_single(StringRef expected_name, uint param_index, uint pindex)
  {
    BLI_assert(m_is_computed);
    UNUSED_VARS_NDEBUG(expected_name);
#ifdef DEBUG
    StringRef actual_name = m_particle_fn.m_computed_names[param_index];
    BLI_assert(expected_name == actual_name);
#endif
    uint corrected_index = m_particle_fn.m_index_mapping[param_index];
    return m_computed_arrays[corrected_index][pindex];
  }

  template<typename T> const T &get_single(StringRef expected_name, uint param_index, uint pindex)
  {
    BLI_assert(m_is_computed);
    UNUSED_VARS_NDEBUG(expected_name);
#ifdef DEBUG
    StringRef actual_name = m_particle_fn.m_computed_names[param_index];
    BLI_assert(expected_name == actual_name);
#endif
    uint corrected_index = m_particle_fn.m_index_mapping[param_index];
    ArrayRef<T> array = m_computed_arrays[corrected_index].as_typed_ref<T>();
    return array[pindex];
  }

  template<typename T>
  ArrayRef<T> get_vector(StringRef expected_name, uint param_index, uint pindex)
  {
    BLI_assert(m_is_computed);
    UNUSED_VARS_NDEBUG(expected_name);
#ifdef DEBUG
    StringRef actual_name = m_particle_fn.m_computed_names[param_index];
    BLI_assert(expected_name == actual_name);
#endif
    uint corrected_index = m_particle_fn.m_index_mapping[param_index];
    GenericVectorArray &vector_array = *m_computed_vector_arrays[corrected_index];
    return vector_array[pindex].as_typed_ref<T>();
  }

  GenericVectorArray &computed_vector_array(uint param_index)
  {
    BLI_assert(m_is_computed);
    uint corrected_index = m_particle_fn.m_index_mapping[param_index];
    return *m_computed_vector_arrays[corrected_index];
  }

  GenericArrayRef computed_array(uint param_index)
  {
    BLI_assert(m_is_computed);
    uint corrected_index = m_particle_fn.m_index_mapping[param_index];
    return m_computed_arrays[corrected_index];
  }
};

}  // namespace BParticles
