#include "particles_state.hpp"

namespace BParticles {

ParticlesState::~ParticlesState()
{
  m_container_by_id.foreach_value([](ParticlesContainer *container) { delete container; });
}

}  // namespace BParticles
