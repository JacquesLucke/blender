#pragma once

#include "forces.hpp"
#include "integrator_interface.hpp"

namespace BParticles {

class ConstantVelocityIntegrator : public Integrator {
  std::unique_ptr<AttributesInfo> m_offset_attributes_info;

 public:
  ConstantVelocityIntegrator();

  const AttributesInfo &offset_attributes_info() override;
  void integrate(IntegratorInterface &interface) override;
};

class EulerIntegrator : public Integrator {
 private:
  std::unique_ptr<AttributesInfo> m_offset_attributes_info;
  Vector<Force *> m_forces;

 public:
  EulerIntegrator(ArrayRef<Force *> forces);
  ~EulerIntegrator();

  const AttributesInfo &offset_attributes_info() override;
  void integrate(IntegratorInterface &interface) override;

 private:
  void compute_combined_force(IntegratorInterface &interface, MutableArrayRef<float3> r_force);

  void compute_offsets(ArrayRef<float> durations,
                       ArrayRef<float3> last_velocities,
                       ArrayRef<float3> combined_force,
                       MutableArrayRef<float3> r_position_offsets,
                       MutableArrayRef<float3> r_velocity_offsets);
};

}  // namespace BParticles
