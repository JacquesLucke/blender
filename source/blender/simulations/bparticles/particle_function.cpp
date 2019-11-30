#include "BLI_temporary_allocator.h"

#include "FN_multi_function_common_contexts.h"

#include "particle_function.hpp"

namespace BParticles {

using FN::CPPType;

ParticleFunctionInputProvider::~ParticleFunctionInputProvider()
{
}

ParticleFunction::ParticleFunction(std::unique_ptr<const MultiFunction> fn,
                                   Vector<ParticleFunctionInputProvider *> input_providers,
                                   FN::ExternalDataCacheContext &data_cache,
                                   FN::PersistentSurfacesLookupContext &persistent_surface_lookup)
    : m_fn(std::move(fn)),
      m_input_providers(std::move(input_providers)),
      m_data_cache(data_cache),
      m_persistent_surface_lookup(persistent_surface_lookup)
{
}

ParticleFunction::~ParticleFunction()
{
  for (auto *provider : m_input_providers) {
    delete provider;
  }
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ActionInterface &interface)
{
  return this->compute(interface.pindices(), interface.attributes(), &interface.context());
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(
    OffsetHandlerInterface &interface)
{
  return this->compute(interface.pindices(), interface.attributes(), nullptr);
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ForceInterface &interface)
{
  return this->compute(interface.pindices(), interface.attributes(), nullptr);
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(EventFilterInterface &interface)
{
  return this->compute(interface.pindices(), interface.attributes(), nullptr);
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ArrayRef<uint> pindices,
                                                                  AttributesRef attributes,
                                                                  ActionContext *action_context)
{
  uint array_size = attributes.size();

  auto result = BLI::make_unique<ParticleFunctionResult>();
  result->m_pindices = pindices;

  FN::MFParamsBuilder params_builder(*m_fn, array_size);
  FN::MFContextBuilder context_builder;

  Vector<GenericMutableArrayRef> arrays_to_free;

  for (uint param_index : m_fn->param_indices()) {
    FN::MFParamType param_type = m_fn->param_type(param_index);
    switch (param_type.type()) {
      case FN::MFParamType::VectorInput:
      case FN::MFParamType::MutableVector:
      case FN::MFParamType::MutableSingle:
        BLI_assert(false);
        break;
      case FN::MFParamType::VectorOutput:
        /*TODO */
        BLI_assert(false);
        break;
      case FN::MFParamType::SingleInput: {
        auto *provider = m_input_providers[param_index];
        InputProviderInterface interface(pindices, attributes, action_context);
        auto optional_array = provider->get(interface);
        const CPPType &type = param_type.data_type().single__cpp_type();
        if (optional_array.has_value()) {
          ParticleFunctionInputArray array = optional_array.extract();
          BLI_assert(array.buffer != nullptr);
          BLI_assert(array.stride > 0);

          GenericMutableArrayRef array_ref{type, array.buffer, array_size};
          params_builder.add_readonly_single_input(array_ref);

          if (array.is_newly_allocated) {
            arrays_to_free.append(array_ref);
          }
        }
        else {
          uint element_size = type.size();
          void *default_buffer = BLI_temporary_allocate(element_size * array_size);
          for (uint pindex : pindices) {
            memset(POINTER_OFFSET(default_buffer, pindex * element_size), 0, element_size);
          }
          GenericMutableArrayRef array_ref{type, default_buffer, array_size};
          params_builder.add_readonly_single_input(array_ref);
          arrays_to_free.append(array_ref);
        }
        break;
      }
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
  context_builder.add_element_context(attributes_context, IndexRange(array_size));
  context_builder.add_element_context(m_data_cache);
  context_builder.add_element_context(m_persistent_surface_lookup);

  m_fn->call(pindices, params_builder, context_builder);

  for (GenericMutableArrayRef array_ref : arrays_to_free) {
    array_ref.destruct_indices(pindices);
    BLI_temporary_deallocate(array_ref.buffer());
  }

  return result;
}

}  // namespace BParticles
