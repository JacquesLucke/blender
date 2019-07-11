#include "action_interface.hpp"

namespace BParticles {

Action::~Action()
{
}

ParticleFunctionCaller ParticleFunction::get_caller(AttributeArrays attributes,
                                                    EventInfo &event_info)
{
  ParticleFunctionCaller caller;
  caller.m_body = m_tuple_call;

  for (uint i = 0; i < m_function->input_amount(); i++) {
    StringRef input_name = m_function->input_name(i);
    void *ptr = nullptr;
    uint stride = 0;
    if (input_name.startswith("Event")) {
      StringRef event_attribute_name = input_name.drop_prefix("Event: ");
      ptr = event_info.get_info_array(event_attribute_name);
      stride = sizeof(float3); /* TODO make not hardcoded */
    }
    else if (input_name.startswith("Attribute")) {
      StringRef attribute_name = input_name.drop_prefix("Attribute: ");
      uint index = attributes.attribute_index(attribute_name);
      stride = attributes.attribute_stride(index);
      ptr = attributes.get_ptr(index);
    }
    else {
      BLI_assert(false);
    }

    BLI_assert(ptr);
    caller.m_attribute_buffers.append(ptr);
    caller.m_strides.append(stride);
  }

  return caller;
}

}  // namespace BParticles
