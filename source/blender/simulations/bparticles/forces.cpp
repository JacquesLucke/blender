#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

Force::~Force()
{
}

class GravityForce : public Force {
 private:
  SharedFunction m_compute_acceleration_fn;
  TupleCallBody *m_compute_acceleration_body;

 public:
  GravityForce(SharedFunction &compute_acceleration_fn)
      : m_compute_acceleration_fn(compute_acceleration_fn)
  {
    m_compute_acceleration_body = m_compute_acceleration_fn->body<TupleCallBody>();
  }

  void add_force(ParticlesBlock &block, ArrayRef<float3> r_force) override
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
};

class TurbulenceForce : public BParticles::Force {
 private:
  float m_strength;

 public:
  TurbulenceForce(float strength) : m_strength(strength)
  {
  }

  void add_force(ParticlesBlock &block, ArrayRef<float3> r_force) override
  {
    auto positions = block.attributes().get_float3("Position");

    for (uint pindex = 0; pindex < block.active_amount(); pindex++) {
      float3 pos = positions[pindex];
      float value = BLI_hnoise(0.5f, pos.x, pos.y, pos.z);
      r_force[pindex].z += value * m_strength;
    }
  }
};

Force *FORCE_gravity(SharedFunction &compute_acceleration_fn)
{
  return new GravityForce(compute_acceleration_fn);
}

Force *FORCE_turbulence(float strength)
{
  return new TurbulenceForce(strength);
}

}  // namespace BParticles
