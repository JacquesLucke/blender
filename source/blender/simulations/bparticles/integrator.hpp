#pragma once

#include "forces.hpp"
#include "step_description.hpp"

namespace BParticles {

class EulerIntegrator : public Integrator {
 private:
  AttributesInfo m_offset_attributes_info;
  SmallVector<Force *> m_forces;

 public:
  EulerIntegrator();
  ~EulerIntegrator();

  AttributesInfo &offset_attributes_info() override;
  void integrate(IntegratorInterface &interface) override;

  void add_force(std::unique_ptr<Force> force)
  {
    m_forces.append(force.release());
  }

 private:
  void compute_combined_force(ParticlesBlock &block, ArrayRef<float3> r_force);

  void compute_offsets(ArrayRef<float> durations,
                       ArrayRef<float3> last_velocities,
                       ArrayRef<float3> combined_force,
                       ArrayRef<float3> r_position_offsets,
                       ArrayRef<float3> r_velocity_offsets);
};

}  // namespace BParticles
