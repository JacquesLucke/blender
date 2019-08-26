#include "FN_functions.hpp"
#include "BLI_temporary_allocator.h"

#include "particle_function.hpp"

namespace BParticles {

ParticleFunctionInputProvider::~ParticleFunctionInputProvider()
{
}

ParticleFunction::ParticleFunction(SharedFunction fn_no_deps,
                                   SharedFunction fn_with_deps,
                                   Vector<ParticleFunctionInputProvider *> input_providers,
                                   Vector<bool> parameter_depends_on_particle)
    : m_fn_no_deps(std::move(fn_no_deps)),
      m_fn_with_deps(std::move(fn_with_deps)),
      m_input_providers(std::move(input_providers)),
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

  if (m_fn_with_deps->output_amount() > 0) {
    m_array_execution = FN::Functions::get_precompiled_array_execution(m_fn_with_deps);
  }
}

ParticleFunction::~ParticleFunction()
{
  for (auto *provider : m_input_providers) {
    delete provider;
  }
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ActionInterface &interface)
{
  return this->compute(interface.particles(),
                       ParticleTimes::FromCurrentTimes(interface.current_times()),
                       &interface.context());
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(
    OffsetHandlerInterface &interface)
{
  return this->compute(interface.particles(),
                       ParticleTimes::FromDurationsAndEnd(interface.remaining_durations(),
                                                          interface.step_end_time()),
                       nullptr);
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ForceInterface &interface)
{
  return this->compute(interface.particles(),
                       ParticleTimes::FromDurationsAndEnd(interface.remaining_durations(),
                                                          interface.step_end_time()),
                       nullptr);
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(EventFilterInterface &interface)
{
  return this->compute(interface.particles(),
                       ParticleTimes::FromDurationsAndEnd(interface.remaining_durations(),
                                                          interface.step_end_time()),
                       nullptr);
}

std::unique_ptr<ParticleFunctionResult> ParticleFunction::compute(ParticleSet particles,
                                                                  ParticleTimes particle_times,
                                                                  ActionContext *action_context)
{
  uint parameter_amount = m_parameter_depends_on_particle.size();

  ParticleFunctionResult *result = new ParticleFunctionResult();
  result->m_buffers.append_n_times(nullptr, parameter_amount);
  result->m_only_first.append_n_times(false, parameter_amount);
  result->m_strides.append_n_times(0, parameter_amount);
  result->m_fn_no_deps = m_fn_no_deps.ptr();
  result->m_fn_with_deps = m_fn_with_deps.ptr();
  result->m_output_indices = m_output_indices;

  this->init_without_deps(result);
  this->init_with_deps(result, particles, particle_times, action_context);

  return std::unique_ptr<ParticleFunctionResult>(result);
}

void ParticleFunction::init_without_deps(ParticleFunctionResult *result)
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

    uint output_stride = type_info.size();
    void *output_buffer = BLI_temporary_allocate(output_stride);

    result->m_buffers[parameter_index] = output_buffer;
    result->m_strides[parameter_index] = output_stride;
    result->m_only_first[parameter_index] = true;

    fn_out.relocate_out__dynamic(output_index, output_buffer);
  }
}

void ParticleFunction::init_with_deps(ParticleFunctionResult *result,
                                      ParticleSet particles,
                                      ParticleTimes particle_times,
                                      ActionContext *action_context)
{
  if (m_fn_with_deps->output_amount() == 0) {
    return;
  }

  uint parameter_amount = m_parameter_depends_on_particle.size();

  Vector<void *> input_buffers;
  Vector<uint> input_sizes;
  Vector<uint> inputs_to_free;

  for (uint i = 0; i < m_fn_with_deps->input_amount(); i++) {
    auto *provider = m_input_providers[i];
    InputProviderInterface interface(particles, particle_times, action_context);
    auto array = provider->get(interface);
    BLI_assert(array.buffer != nullptr);
    BLI_assert(array.stride > 0);

    input_buffers.append(array.buffer);
    input_sizes.append(array.stride);
    if (array.is_newly_allocated) {
      inputs_to_free.append(i);
    }
  }

  Vector<void *> output_buffers;
  Vector<uint> output_sizes;

  for (uint parameter_index = 0; parameter_index < parameter_amount; parameter_index++) {
    if (!m_parameter_depends_on_particle[parameter_index]) {
      continue;
    }

    uint output_index = m_output_indices[parameter_index];
    CPPTypeInfo &type_info = m_fn_with_deps->output_type(output_index)->extension<CPPTypeInfo>();

    uint output_stride = type_info.size();
    void *output_buffer = BLI_temporary_allocate(output_stride * particles.size());

    result->m_buffers[parameter_index] = output_buffer;
    result->m_strides[parameter_index] = output_stride;
    result->m_only_first[parameter_index] = false;

    output_buffers.append(output_buffer);
    output_sizes.append(output_stride);
  }

  ExecutionStack stack;
  ExecutionContext execution_context(stack);
  FN::TextStackFrame stack_frame("Particle Function");
  stack.push(&stack_frame);
  m_array_execution->call(particles.pindices(), input_buffers, output_buffers, execution_context);
  stack.pop();

  for (uint i : inputs_to_free) {
    void *buffer = input_buffers[i];
    BLI_temporary_deallocate(buffer);
  }
}

}  // namespace BParticles
