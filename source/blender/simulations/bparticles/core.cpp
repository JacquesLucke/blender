#include "core.hpp"

namespace BParticles {

Force::~Force()
{
}

Emitter::~Emitter()
{
}

Action::~Action()
{
}

Event::~Event()
{
}

ParticleInfluences::~ParticleInfluences()
{
}

StepDescription::~StepDescription()
{
}

ParticlesState::~ParticlesState()
{
  delete m_container;
}

}  // namespace BParticles
