#include "action_interface.hpp"

namespace BParticles {

Action::~Action()
{
}

void ActionInterface::execute_action_for_subset(ArrayRef<uint> indices,
                                                std::unique_ptr<Action> &action)
{
  EventExecuteInterface &interface = m_event_execute_interface;

  ParticleSet &particles = interface.particles();
  auto current_times = interface.current_times();

  SmallVector<float> sub_current_times;
  SmallVector<uint> particle_indices;
  for (uint i : indices) {
    particle_indices.append(particles.get_particle_index(i));
    sub_current_times.append(current_times[i]);
  }

  ParticleSet sub_particles(particles.block(), particle_indices);
  EventExecuteInterface sub_execute_interface(sub_particles,
                                              interface.block_allocator(),
                                              sub_current_times,
                                              interface.event_storage(),
                                              interface.attribute_offsets(),
                                              interface.step_end_time());
  ActionInterface sub_interface(sub_execute_interface, m_event_info);
  action->execute(sub_interface);
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
