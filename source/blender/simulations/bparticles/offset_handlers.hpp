#pragma once

#include "step_description.hpp"

namespace BParticles {

class CreateTrailHandler : public OffsetHandler {
 private:
  std::string m_particle_type_name;
  float m_rate;

 public:
  CreateTrailHandler(StringRef particle_type_name, float rate)
      : m_particle_type_name(particle_type_name.to_std_string()), m_rate(rate)
  {
  }

  void execute(OffsetHandlerInterface &interface) override;
};

}  // namespace BParticles
