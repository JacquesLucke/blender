#include "particles_state.hpp"

namespace BParticles {

ParticlesState::~ParticlesState()
{
  for (ParticlesContainer *container : m_container_by_id.values()) {
    delete container;
  }
}

}  // namespace BParticles
