#include "particle_function.hpp"

namespace BParticles {

ParticleFunctionCaller ParticleFunction::get_caller(AttributeArrays attributes)
{
  ParticleFunctionCaller caller;
  caller.m_body = m_body;
  caller.m_min_buffer_length = attributes.size();

  for (uint i = 0; i < m_fn->input_amount(); i++) {
    StringRef input_name = m_fn->input_name(i);
    void *input_buffer = nullptr;
    uint input_stride = 0;
    if (input_name.startswith("Attribute")) {
      StringRef attribute_name = input_name.drop_prefix("Attribute: ");
      uint attribute_index = attributes.attribute_index(attribute_name);
      input_buffer = attributes.get_ptr(attribute_index);
      input_stride = attributes.attribute_stride(attribute_index);
    }
    else {
      BLI_assert(false);
    }
    BLI_assert(input_buffer != nullptr);

    caller.m_input_buffers.append(input_buffer);
    caller.m_input_strides.append(input_stride);
  }

  return caller;
}

}  // namespace BParticles
