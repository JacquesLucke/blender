#include "integrator.hpp"

namespace BParticles {

ConstantVelocityIntegrator::ConstantVelocityIntegrator()
{
  AttributesDeclaration builder;
  builder.add_float3("Position", {0, 0, 0});
  m_offset_attributes_info = AttributesInfo(builder);
}

AttributesInfo &ConstantVelocityIntegrator::offset_attributes_info()
{
  return m_offset_attributes_info;
}

void ConstantVelocityIntegrator::integrate(IntegratorInterface &interface)
{
  ParticlesBlock &block = interface.block();
  auto velocities = block.attributes().get<float3>("Velocity");
  auto position_offsets = interface.attribute_offsets().get<float3>("Position");
  auto durations = interface.remaining_durations();

  for (uint pindex = 0; pindex < block.active_amount(); pindex++) {
    position_offsets[pindex] = velocities[pindex] * durations[pindex];
  }
}

EulerIntegrator::EulerIntegrator(ArrayRef<Force *> forces) : m_forces(forces)
{
  AttributesDeclaration builder;
  builder.add_float3("Position", {0, 0, 0});
  builder.add_float3("Velocity", {0, 0, 0});
  m_offset_attributes_info = AttributesInfo(builder);
}

EulerIntegrator::~EulerIntegrator()
{
  for (Force *force : m_forces) {
    delete force;
  }
}

AttributesInfo &EulerIntegrator::offset_attributes_info()
{
  return m_offset_attributes_info;
}

void EulerIntegrator::integrate(IntegratorInterface &interface)
{
  AttributeArrays r_offsets = interface.attribute_offsets();
  ArrayRef<float> durations = interface.remaining_durations();

  ArrayAllocator::Array<float3> combined_force(interface.array_allocator());
  this->compute_combined_force(interface, combined_force);

  auto last_velocities = interface.block().attributes().get<float3>("Velocity");

  auto position_offsets = r_offsets.get<float3>("Position");
  auto velocity_offsets = r_offsets.get<float3>("Velocity");
  this->compute_offsets(
      durations, last_velocities, combined_force, position_offsets, velocity_offsets);
}

BLI_NOINLINE void EulerIntegrator::compute_combined_force(IntegratorInterface &interface,
                                                          ArrayRef<float3> r_force)
{
  r_force.fill({0, 0, 0});

  ForceInterface force_interface(interface.step_data(), r_force);

  for (Force *force : m_forces) {
    force->add_force(force_interface);
  }
}

BLI_NOINLINE void EulerIntegrator::compute_offsets(ArrayRef<float> durations,
                                                   ArrayRef<float3> last_velocities,
                                                   ArrayRef<float3> combined_force,
                                                   ArrayRef<float3> r_position_offsets,
                                                   ArrayRef<float3> r_velocity_offsets)
{
  uint amount = durations.size();
  for (uint pindex = 0; pindex < amount; pindex++) {
    float mass = 1.0f;
    float duration = durations[pindex];

    r_velocity_offsets[pindex] = duration * combined_force[pindex] / mass;
    r_position_offsets[pindex] = duration *
                                 (last_velocities[pindex] + r_velocity_offsets[pindex] * 0.5f);
  }
}

}  // namespace BParticles
