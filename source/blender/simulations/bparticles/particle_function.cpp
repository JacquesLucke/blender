#include "particle_function.hpp"

namespace BParticles {

ParticleFunctionCaller ParticleFunction::get_caller(ActionInterface &action_interface)
{
  return this->get_caller(action_interface.array_allocator(),
                          action_interface.particles().attributes(),
                          &action_interface.context());
}

ParticleFunctionCaller ParticleFunction::get_caller(
    OffsetHandlerInterface &offset_handler_interface)
{
  return this->get_caller(offset_handler_interface.array_allocator(),
                          offset_handler_interface.particles().attributes(),
                          nullptr);
}

ParticleFunctionCaller ParticleFunction::get_caller(ArrayAllocator &array_allocator,
                                                    AttributeArrays attributes,
                                                    ActionContext *action_context)
{
  ParticleFunctionCaller caller;
  caller.m_body = m_body;
  caller.m_min_buffer_length = attributes.size();
  caller.m_array_allocator = &array_allocator;

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
    else if (action_context != nullptr && input_name.startswith("Action Context")) {
      StringRef context_name = input_name.drop_prefix("Action Context: ");
      ActionContext::ContextArray array = action_context->get_context_array(context_name);
      input_buffer = array.buffer;
      input_stride = array.stride;
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
