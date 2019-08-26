#pragma once

#include "simulation_state.hpp"

namespace BParticles {

class StepSimulator {
 public:
  virtual ~StepSimulator()
  {
  }

  virtual void simulate(SimulationState &simulation_state) = 0;
};

}  // namespace BParticles
