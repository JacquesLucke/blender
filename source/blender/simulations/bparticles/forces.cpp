#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

Force::~Force()
{
}

void GravityForce::add_force(ForceInterface &interface)
{
  ParticlesBlock &block = interface.block();
  ArrayRef<float3> destination = interface.combined_destination();

  FN_TUPLE_CALL_ALLOC_TUPLES(m_compute_acceleration_body, fn_in, fn_out);

  FN::ExecutionStack stack;
  FN::ExecutionContext execution_context(stack);

  m_compute_acceleration_body->call(fn_in, fn_out, execution_context);

  float3 acceleration = fn_out.get<float3>(0);

  for (uint i = 0; i < block.active_amount(); i++) {
    destination[i] += acceleration;
  }
};

void TurbulenceForce::add_force(ForceInterface &interface)
{
  ParticlesBlock &block = interface.block();
  ArrayRef<float3> destination = interface.combined_destination();

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
    destination[pindex] += {x, y, z};
  }
}

}  // namespace BParticles
