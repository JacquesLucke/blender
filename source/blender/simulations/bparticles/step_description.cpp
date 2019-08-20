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
  m_types.foreach_value([](ParticleType *type) { delete type; });

  for (Emitter *emitter : m_emitters) {
    delete emitter;
  }
}

}  // namespace BParticles
