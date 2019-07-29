#include "particle_function.hpp"

namespace BParticles {

ParticleFunction::ParticleFunction(SharedFunction fn_no_deps,
                                   SharedFunction fn_with_deps,
                                   Vector<bool> parameter_depends_on_particle)
    : m_fn_no_deps(std::move(fn_no_deps)),
      m_fn_with_deps(std::move(fn_with_deps)),
      m_parameter_depends_on_particle(std::move(parameter_depends_on_particle))
{
  BLI_assert(m_fn_no_deps->output_amount() + m_fn_with_deps->output_amount() ==
             m_parameter_depends_on_particle.size());
  BLI_assert(m_fn_no_deps->input_amount() == 0);

  uint no_deps_index = 0;
  uint with_deps_index = 0;
  for (uint i = 0; i < m_parameter_depends_on_particle.size(); i++) {
    if (m_parameter_depends_on_particle[i]) {
      m_output_indices.append(with_deps_index);
      with_deps_index++;
    }
    else {
      m_output_indices.append(no_deps_index);
      no_deps_index++;
    }
  }
}

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
  uint parameter_amount = m_parameter_depends_on_particle.size();

  ParticleFunctionResult *result = new ParticleFunctionResult();
  result->m_array_allocator = &array_allocator;
  result->m_buffers.append_n_times(nullptr, parameter_amount);
  result->m_only_first.append_n_times(false, parameter_amount);
  result->m_strides.append_n_times(0, parameter_amount);
  result->m_fn_no_deps = m_fn_no_deps.ptr();
  result->m_fn_with_deps = m_fn_with_deps.ptr();
  result->m_output_indices = m_output_indices;

  this->init_without_deps(result, array_allocator);
  this->init_with_deps(result, array_allocator, pindices, attributes, action_context);

  return std::unique_ptr<ParticleFunctionResult>(result);
}

void ParticleFunction::init_without_deps(ParticleFunctionResult *result,
                                         ArrayAllocator &array_allocator)
{
  if (m_fn_no_deps->output_amount() == 0) {
    return;
  }

  uint parameter_amount = m_parameter_depends_on_particle.size();
  TupleCallBody &body = m_fn_no_deps->body<TupleCallBody>();

  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body.call__setup_execution_context(fn_in, fn_out);

  for (uint parameter_index = 0; parameter_index < parameter_amount; parameter_index++) {
    if (m_parameter_depends_on_particle[parameter_index]) {
      continue;
    }
    uint output_index = m_output_indices[parameter_index];
    CPPTypeInfo &type_info = m_fn_no_deps->output_type(output_index)->extension<CPPTypeInfo>();

    uint output_stride = type_info.size_of_type();
    void *output_buffer = array_allocator.allocate(output_stride);

    result->m_buffers[parameter_index] = output_buffer;
    result->m_strides[parameter_index] = output_stride;
    result->m_only_first[parameter_index] = true;

    fn_out.relocate_out__dynamic(output_index, output_buffer);
  }
}

void ParticleFunction::init_with_deps(ParticleFunctionResult *result,
                                      ArrayAllocator &array_allocator,
                                      ArrayRef<uint> pindices,
                                      AttributeArrays attributes,
                                      ActionContext *action_context)
{
  if (m_fn_with_deps->output_amount() == 0) {
    return;
  }

  uint parameter_amount = m_parameter_depends_on_particle.size();
  TupleCallBody &body = m_fn_with_deps->body<TupleCallBody>();

  Vector<void *> input_buffers;
  Vector<uint> input_strides;

  for (uint i = 0; i < m_fn_with_deps->input_amount(); i++) {
    StringRef input_name = m_fn_with_deps->input_name(i);
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

  Vector<void *> output_buffers;
  Vector<uint> output_strides;

  for (uint parameter_index = 0; parameter_index < parameter_amount; parameter_index++) {
    if (!m_parameter_depends_on_particle[parameter_index]) {
      continue;
    }

    uint output_index = m_output_indices[parameter_index];
    CPPTypeInfo &type_info = m_fn_with_deps->output_type(output_index)->extension<CPPTypeInfo>();

    uint output_stride = type_info.size_of_type();
    void *output_buffer = array_allocator.allocate(output_stride);

    result->m_buffers[parameter_index] = output_buffer;
    result->m_strides[parameter_index] = output_stride;
    result->m_only_first[parameter_index] = false;

    output_buffers.append(output_buffer);
    output_strides.append(output_stride);
  }

  ExecutionStack stack;
  ExecutionContext execution_context(stack);

  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);

  for (uint pindex : pindices) {
    for (uint i = 0; i < input_buffers.size(); i++) {
      void *ptr = POINTER_OFFSET(input_buffers[i], pindex * input_strides[i]);
      fn_in.copy_in__dynamic(i, ptr);
    }

    body.call(fn_in, fn_out, execution_context);

    for (uint i = 0; i < output_buffers.size(); i++) {
      void *ptr = POINTER_OFFSET(output_buffers[i], pindex * output_strides[i]);
      fn_out.relocate_out__dynamic(i, ptr);
    }
  }
}

}  // namespace BParticles
