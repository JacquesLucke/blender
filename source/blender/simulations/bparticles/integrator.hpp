#pragma once

#include "forces.hpp"
#include "step_description.hpp"

namespace BParticles {

class ConstantVelocityIntegrator : public Integrator {
  AttributesInfo m_offset_attributes_info;

 public:
  ConstantVelocityIntegrator();

  AttributesInfo &offset_attributes_info() override;
  void integrate(IntegratorInterface &interface) override;
};

class EulerIntegrator : public Integrator {
 private:
  AttributesInfo m_offset_attributes_info;
  Vector<Force *> m_forces;

 public:
  EulerIntegrator(ArrayRef<Force *> forces);
  ~EulerIntegrator();

  AttributesInfo &offset_attributes_info() override;
  void integrate(IntegratorInterface &interface) override;

 private:
  void compute_combined_force(ParticlesBlock &block, ArrayRef<float3> r_force);

  void compute_offsets(ArrayRef<float> durations,
                       ArrayRef<float3> last_velocities,
                       ArrayRef<float3> combined_force,
                       ArrayRef<float3> r_position_offsets,
                       ArrayRef<float3> r_velocity_offsets);
};

}  // namespace BParticles
