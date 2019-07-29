#pragma once

#include "step_description.hpp"
#include "particle_function.hpp"

namespace BParticles {

class CreateTrailHandler : public OffsetHandler {
 private:
  std::string m_particle_type_name;
  std::unique_ptr<ParticleFunction> m_compute_inputs;

 public:
  CreateTrailHandler(StringRef particle_type_name,
                     std::unique_ptr<ParticleFunction> compute_inputs)
      : m_particle_type_name(particle_type_name.to_std_string()),
        m_compute_inputs(std::move(compute_inputs))
  {
  }

  void execute(OffsetHandlerInterface &interface) override;
};

}  // namespace BParticles
