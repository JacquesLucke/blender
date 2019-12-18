#include "BLI_temporary_allocator.h"

#include "FN_multi_function_common_contexts.h"

#include "particle_function.hpp"

namespace BParticles {

using FN::CPPType;

ParticleFunction::ParticleFunction(const MultiFunction &fn,
                                   const BKE::IDDataCache &id_data_cache,
                                   const BKE::IDHandleLookup &id_handle_lookup)
    : m_fn(fn), m_id_data_cache(id_data_cache), m_id_handle_lookup(id_handle_lookup)
{
}

ParticleFunction::~ParticleFunction()
{
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ActionInterface &interface)
{
  return this->compute(interface.pindices(), interface.attributes());
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(
    OffsetHandlerInterface &interface)
{
  return this->compute(interface.pindices(), interface.attributes());
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ForceInterface &interface)
{
  return this->compute(interface.pindices(), interface.attributes());
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(EventFilterInterface &interface)
{
  return this->compute(interface.pindices(), interface.attributes());
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ArrayRef<uint> pindices,
                                                                  AttributesRef attributes)
{
  uint array_size = attributes.size();

  auto result = BLI::make_unique<ParticleFunctionResult>();
  result->m_pindices = pindices;

  FN::MFParamsBuilder params_builder(m_fn, array_size);

  Vector<GenericMutableArrayRef> arrays_to_free;

  for (uint param_index : m_fn.param_indices()) {
    FN::MFParamType param_type = m_fn.param_type(param_index);
    switch (param_type.type()) {
      case FN::MFParamType::SingleInput:
      case FN::MFParamType::VectorInput:
      case FN::MFParamType::MutableVector:
      case FN::MFParamType::MutableSingle:
        BLI_assert(false);
        break;
      case FN::MFParamType::VectorOutput:
        /*TODO */
        BLI_assert(false);
        break;
      case FN::MFParamType::SingleOutput: {
        const CPPType &type = param_type.data_type().single__cpp_type();
        void *output_buffer = BLI_temporary_allocate(type.size() * array_size);
        params_builder.add_single_output(
            FN::GenericMutableArrayRef(type, output_buffer, array_size));
        result->m_computed_buffers.append(GenericMutableArrayRef(type, output_buffer, array_size));
        break;
      }
    }
  }

  FN::ParticleAttributesContext attributes_context(attributes);

  FN::MFContextBuilder context_builder;
  context_builder.add_global_context(m_id_handle_lookup);
  context_builder.add_global_context(m_id_data_cache);
  context_builder.add_element_context(attributes_context, IndexRange(array_size));

  m_fn.call(pindices, params_builder, context_builder);

  for (GenericMutableArrayRef array_ref : arrays_to_free) {
    array_ref.destruct_indices(pindices);
    BLI_temporary_deallocate(array_ref.buffer());
  }

  return result;
}

}  // namespace BParticles
