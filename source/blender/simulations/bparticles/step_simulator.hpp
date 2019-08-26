#pragma once

#include "simulation_state.hpp"

namespace BParticles {

class StepSimulator {
 public:
  virtual void simulate(SimulationState &simulation_state, float time_step) const = 0;
};

}  // namespace BParticles
