#pragma once

#include "particles_state.hpp"
#include "world_state.hpp"

namespace BParticles {

class SimulationState {
 private:
  ParticlesState m_particles;
  WorldState m_world;

 public:
  ParticlesState &particles()
  {
    return m_particles;
  }

  WorldState &world()
  {
    return m_world;
  }
};

}  // namespace BParticles
