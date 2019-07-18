#pragma once

#include "core.hpp"

namespace BParticles {

class ParticleTypeBuilder {
 private:
  Integrator *m_integrator;
  SmallVector<Event *> m_events;
  SmallVector<OffsetHandler *> m_offset_handlers;
  AttributesDeclaration m_attributes;

 public:
  ParticleTypeBuilder() = default;

  void set_integrator(Integrator *integrator)
  {
    m_integrator = integrator;
  }

  void add_event(std::unique_ptr<Event> event)
  {
    m_events.append(event.release());
  }

  void add_offset_handler(std::unique_ptr<OffsetHandler> offset_handler)
  {
    m_offset_handlers.append(offset_handler.release());
  }

  AttributesDeclaration &attributes()
  {
    return m_attributes;
  }

  ParticleType *build()
  {
    BLI_assert(m_integrator);
    ParticleType *type = new ParticleType(m_attributes, m_integrator, m_events, m_offset_handlers);
    m_integrator = nullptr;
    m_events.clear();
    m_offset_handlers.clear();
    return type;
  }
};

class StepDescriptionBuilder {
 private:
  StringMap<ParticleTypeBuilder *> m_type_builders;
  SmallVector<Emitter *> m_emitters;

 public:
  void add_emitter(std::unique_ptr<Emitter> emitter)
  {
    m_emitters.append(emitter.release());
  }

  ParticleTypeBuilder &get_type(StringRef name)
  {
    return *m_type_builders.lookup(name);
  }

  ParticleTypeBuilder &add_type(StringRef name)
  {
    ParticleTypeBuilder *builder = new ParticleTypeBuilder();
    m_type_builders.add_new(name, builder);
    return *builder;
  }

  bool has_type(StringRef name)
  {
    return m_type_builders.contains(name);
  }

  std::unique_ptr<StepDescription> build(float duration)
  {
    StringMap<ParticleType *> types;
    for (auto item : m_type_builders.items()) {
      types.add_new(item.key, item.value->build());
    }

    StepDescription *step_description = new StepDescription(duration, types, m_emitters);
    return std::unique_ptr<StepDescription>(step_description);
  }
};

}  // namespace BParticles
