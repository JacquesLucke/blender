#include "particle_function.hpp"

namespace BParticles {

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ActionInterface &interface)
{
  return this->compute(interface.array_allocator(),
                       interface.particles().pindices(),
                       interface.particles().attributes(),
                       &interface.context());
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(
    OffsetHandlerInterface &interface)
{
  return this->compute(interface.array_allocator(),
                       interface.particles().pindices(),
                       interface.particles().attributes(),
                       nullptr);
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ForceInterface &interface)
{
  ParticlesBlock &block = interface.block();
  return this->compute(interface.array_allocator(),
                       block.active_range().as_array_ref(),
                       block.attributes(),
                       nullptr);
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(EventFilterInterface &interface)
{
  return this->compute(interface.array_allocator(),
                       interface.particles().pindices(),
                       interface.particles().attributes(),
                       nullptr);
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ArrayAllocator &array_allocator,
                                                                  ArrayRef<uint> pindices,
                                                                  AttributeArrays attributes,
                                                                  ActionContext *action_context)
{
  if (m_fn->input_amount() == 0) {
    BLI_assert(array_allocator.array_size() >= 1);

    ParticleFunctionResult *result = new ParticleFunctionResult();
    result->m_fn = m_fn.ptr();
    result->m_array_allocator = &array_allocator;

    FN_TUPLE_CALL_ALLOC_TUPLES(*m_body, fn_in, fn_out);
    m_body->call__setup_execution_context(fn_in, fn_out);

    for (uint i = 0; i < m_fn->output_amount(); i++) {
      CPPTypeInfo &type_info = m_fn->output_type(i)->extension<CPPTypeInfo>();

      uint output_stride = type_info.size_of_type();
      void *output_buffer = array_allocator.allocate(output_stride);

      result->m_buffers.append(output_buffer);
      result->m_strides.append(output_stride);
      result->m_only_first.append(true);

      fn_out.relocate_out__dynamic(i, output_buffer);
    }

    return std::unique_ptr<ParticleFunctionResult>(result);
  }
  else {
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

    ParticleFunctionResult *result = new ParticleFunctionResult();
    result->m_fn = m_fn.ptr();
    result->m_array_allocator = &array_allocator;

    for (uint i = 0; i < m_fn->output_amount(); i++) {
      CPPTypeInfo &type_info = m_fn->output_type(i)->extension<CPPTypeInfo>();

      uint output_stride = type_info.size_of_type();
      void *output_buffer = array_allocator.allocate(output_stride);

      result->m_buffers.append(output_buffer);
      result->m_strides.append(output_stride);
      result->m_only_first.append(false);
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

      for (uint i = 0; i < result->m_buffers.size(); i++) {
        void *ptr = POINTER_OFFSET(result->m_buffers[i], pindex * result->m_strides[i]);
        fn_out.relocate_out__dynamic(i, ptr);
      }
    }

    return std::unique_ptr<ParticleFunctionResult>(result);
  }
}

}  // namespace BParticles
