#pragma once

#include "core.hpp"

namespace BParticles {

class ModifierParticleType : public ParticleType {
 public:
  SmallVector<Event *> m_events;
  SmallVector<OffsetHandler *> m_offset_handlers;
  Integrator *m_integrator;
  AttributesDeclaration m_attributes;

  ~ModifierParticleType()
  {
    delete m_integrator;

    for (Event *event : m_events) {
      delete event;
    }
  }

  ArrayRef<Event *> events() override
  {
    return m_events;
  }

  ArrayRef<OffsetHandler *> offset_handlers() override
  {
    return m_offset_handlers;
  }

  Integrator &integrator() override
  {
    return *m_integrator;
  }

  AttributesDeclaration &attributes() override
  {
    return m_attributes;
  }
};

class ModifierStepDescription : public StepDescription {
 public:
  float m_duration;
  StringMap<ParticleType *> m_types;
  SmallVector<Emitter *> m_emitters;

  ~ModifierStepDescription()
  {
    for (auto *type : m_types.values()) {
      delete type;
    }
    for (Emitter *emitter : m_emitters) {
      delete emitter;
    }
  }

  float step_duration() override
  {
    return m_duration;
  }

  ArrayRef<Emitter *> emitters() override
  {
    return m_emitters;
  }

  StringMap<ParticleType *> &particle_types() override
  {
    return m_types;
  }
};

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
    ModifierParticleType *type = new ModifierParticleType();
    type->m_integrator = m_integrator;
    type->m_events = m_events;
    type->m_offset_handlers = m_offset_handlers;
    type->m_attributes = m_attributes;
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
    ModifierStepDescription *step_description = new ModifierStepDescription();
    step_description->m_duration = duration;
    step_description->m_emitters = m_emitters;
    for (auto item : m_type_builders.items()) {
      step_description->m_types.add_new(item.key, item.value->build());
    }
    return std::unique_ptr<StepDescription>(step_description);
  }
};

}  // namespace BParticles
