#include "particle_function.hpp"

namespace BParticles {

ParticleFunctionResult ParticleFunction::compute(ActionInterface &action_interface)
{
  return this->compute(action_interface.array_allocator(),
                       action_interface.particles().pindices(),
                       action_interface.particles().attributes(),
                       &action_interface.context());
}

ParticleFunctionResult ParticleFunction::compute(OffsetHandlerInterface &offset_handler_interface)
{
  return this->compute(offset_handler_interface.array_allocator(),
                       offset_handler_interface.particles().pindices(),
                       offset_handler_interface.particles().attributes(),
                       nullptr);
}

ParticleFunctionResult ParticleFunction::compute(ForceInterface &force_interface)
{
  ParticlesBlock &block = force_interface.block();
  return this->compute(force_interface.array_allocator(),
                       block.active_range().as_array_ref(),
                       block.attributes(),
                       nullptr);
}

ParticleFunctionResult ParticleFunction::compute(ArrayAllocator &array_allocator,
                                                 ArrayRef<uint> pindices,
                                                 AttributeArrays attributes,
                                                 ActionContext *action_context)
{
  Vector<void *> input_buffers;
  Vector<uint> input_strides;

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

    input_buffers.append(input_buffer);
    input_strides.append(input_stride);
  }

  ParticleFunctionResult result;
  result.m_fn = m_fn.ptr();
  result.m_array_allocator = &array_allocator;

  for (uint i = 0; i < m_fn->output_amount(); i++) {
    CPPTypeInfo *type_info = m_fn->output_type(i)->extension<CPPTypeInfo>();
    BLI_assert(type_info != nullptr);

    uint output_stride = type_info->size_of_type();
    void *output_buffer = array_allocator.allocate(output_stride);

    result.m_buffers.append(output_buffer);
    result.m_strides.append(output_stride);
  }

  ExecutionStack stack;
  ExecutionContext execution_context(stack);

  FN_TUPLE_CALL_ALLOC_TUPLES(*m_body, fn_in, fn_out);

  for (uint pindex : pindices) {
    for (uint i = 0; i < input_buffers.size(); i++) {
      void *ptr = POINTER_OFFSET(input_buffers[i], pindex * input_strides[i]);
      fn_in.copy_in__dynamic(i, ptr);
    }

    m_body->call(fn_in, fn_out, execution_context);

    for (uint i = 0; i < result.m_buffers.size(); i++) {
      void *ptr = POINTER_OFFSET(result.m_buffers[i], pindex * result.m_strides[i]);
      fn_out.relocate_out__dynamic(i, ptr);
    }
  }

  return result;
}

}  // namespace BParticles
