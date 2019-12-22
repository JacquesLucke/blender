#include "BLI_temporary_allocator.h"

#include "FN_multi_function_common_contexts.h"

#include "particle_function.hpp"

namespace BParticles {

using FN::CPPType;
using FN::IndexMask;
using FN::MFContextBuilder;
using FN::MFDataType;
using FN::MFParamsBuilder;
using FN::MFParamType;

ParticleFunction::ParticleFunction(const MultiFunction &fn,
                                   Vector<std::string> computed_names,
                                   const BKE::IDDataCache &id_data_cache,
                                   const BKE::IDHandleLookup &id_handle_lookup)
    : m_fn(fn),
      m_computed_names(computed_names),
      m_id_data_cache(id_data_cache),
      m_id_handle_lookup(id_handle_lookup)
{
  uint single_count = 0;
  uint vector_count = 0;

  for (uint param_index : fn.param_indices()) {
    MFParamType param_type = fn.param_type(param_index);
    BLI_assert(param_type.is_output());
    switch (param_type.data_type().category()) {
      case MFDataType::Single: {
        m_index_mapping.append(single_count++);
        break;
      }
      case MFDataType::Vector: {
        m_index_mapping.append(vector_count++);
        break;
      }
    }
  }
}

ParticleFunctionEvaluator::~ParticleFunctionEvaluator()
{
  BLI_assert(m_is_computed);
  for (GenericVectorArray *vector_array : m_computed_vector_arrays) {
    delete vector_array;
  }
  for (GenericMutableArrayRef array : m_computed_arrays) {
    array.destruct_indices(m_mask.indices());
    MEM_freeN(array.buffer());
  }
}

void ParticleFunctionEvaluator::compute()
{
  BLI_assert(!m_is_computed);

  uint array_size = m_mask.min_array_size();

  FN::ParticleAttributesContext attributes_context = {m_particle_attributes};
  m_context_builder.add_element_context(attributes_context,
                                        FN::MFElementContextIndices::FromDirectMapping());
  m_context_builder.add_global_context(m_particle_fn.m_id_data_cache);
  m_context_builder.add_global_context(m_particle_fn.m_id_handle_lookup);

  const MultiFunction &fn = m_particle_fn.fn();
  MFParamsBuilder params_builder(fn, array_size);
  for (uint param_index : fn.param_indices()) {
    MFParamType param_type = fn.param_type(param_index);
    MFDataType data_type = param_type.data_type();
    BLI_assert(param_type.is_output());
    switch (data_type.category()) {
      case MFDataType::Single: {
        const CPPType &type = data_type.single__cpp_type();
        void *buffer = MEM_mallocN_aligned(array_size * type.size(), type.alignment(), __func__);
        GenericMutableArrayRef array{type, buffer, array_size};
        params_builder.add_single_output(array);
        m_computed_arrays.append(array);
        break;
      }
      case MFDataType::Vector: {
        const CPPType &base_type = data_type.vector__cpp_base_type();
        GenericVectorArray *vector_array = new GenericVectorArray(base_type, array_size);
        params_builder.add_vector_output(*vector_array);
        m_computed_vector_arrays.append(vector_array);
        break;
      }
    }
  }

  fn.call(m_mask, params_builder, m_context_builder);
  m_is_computed = true;
}

}  // namespace BParticles
