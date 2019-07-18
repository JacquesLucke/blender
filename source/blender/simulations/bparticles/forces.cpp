#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

Force::~Force()
{
}

void GravityForce::add_force(ParticlesBlock &block, ArrayRef<float3> r_force)
{
  FN_TUPLE_CALL_ALLOC_TUPLES(m_compute_acceleration_body, fn_in, fn_out);

  FN::ExecutionStack stack;
  FN::ExecutionContext execution_context(stack);

  m_compute_acceleration_body->call(fn_in, fn_out, execution_context);

  float3 acceleration = fn_out.get<float3>(0);

  for (uint i = 0; i < block.active_amount(); i++) {
    r_force[i] += acceleration;
  }
};

void TurbulenceForce::add_force(ParticlesBlock &block, ArrayRef<float3> r_force)
{
  auto positions = block.attributes().get_float3("Position");

  FN_TUPLE_CALL_ALLOC_TUPLES(m_compute_strength_body, fn_in, fn_out);
  FN::ExecutionStack stack;
  FN::ExecutionContext execution_context(stack);
  m_compute_strength_body->call(fn_in, fn_out, execution_context);

  float3 strength = fn_out.get<float3>(0);

  for (uint pindex = 0; pindex < block.active_amount(); pindex++) {
    float3 pos = positions[pindex];
    float x = (BLI_gNoise(0.5f, pos.x, pos.y, pos.z + 1000.0f, false, 1) - 0.5f) * strength.x;
    float y = (BLI_gNoise(0.5f, pos.x, pos.y + 1000.0f, pos.z, false, 1) - 0.5f) * strength.y;
    float z = (BLI_gNoise(0.5f, pos.x + 1000.0f, pos.y, pos.z, false, 1) - 0.5f) * strength.z;
    r_force[pindex] += {x, y, z};
  }
}

void CreateTrailHandler::execute(OffsetHandlerInterface &interface)
{
  if (m_rate <= 0.0f) {
    return;
  }

  ParticleSet particles = interface.particles();
  auto positions = particles.attributes().get_float3("Position");
  auto position_offsets = interface.offsets().get_float3("Position");

  float frequency = 1.0f / m_rate;

  SmallVector<float3> new_positions;
  SmallVector<float> new_birth_times;
  for (uint pindex : particles.pindices()) {
    TimeSpan time_span = interface.time_span(pindex);
    float current_time = frequency * (std::floor(time_span.start() / frequency) + 1.0f);
    while (current_time < time_span.end()) {
      float factor = time_span.get_factor_safe(current_time);
      new_positions.append(positions[pindex] + position_offsets[pindex] * factor);
      new_birth_times.append(current_time);
      current_time += frequency;
    }
  }

  auto new_particles = interface.particle_allocator().request(m_particle_type_name,
                                                              new_positions.size());
  new_particles.set_float3("Position", new_positions);
  new_particles.set_float("Birth Time", new_birth_times);
}

}  // namespace BParticles
