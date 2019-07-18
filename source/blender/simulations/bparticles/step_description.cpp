#include "step_description.hpp"

namespace BParticles {

Emitter::~Emitter()
{
}

Integrator::~Integrator()
{
}

Event::~Event()
{
}

void Event::attributes(AttributesDeclaration &UNUSED(interface))
{
}

OffsetHandler::~OffsetHandler()
{
}

ParticleType::~ParticleType()
{
  delete m_integrator;

  for (Event *event : m_events) {
    delete event;
  }
  for (OffsetHandler *handler : m_offset_handlers) {
    delete handler;
  }
}

StepDescription::~StepDescription()
{
  for (auto *type : m_types.values()) {
    delete type;
  }
  for (Emitter *emitter : m_emitters) {
    delete emitter;
  }
}

}  // namespace BParticles
