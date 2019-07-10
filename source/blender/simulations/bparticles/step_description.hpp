#pragma once

#include "core.hpp"

namespace BParticles {

class ModifierParticleType : public ParticleType {
 public:
  SmallVector<Event *> m_events;
  Integrator *m_integrator;

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

  Integrator &integrator() override
  {
    return *m_integrator;
  }

  void attributes(TypeAttributeInterface &interface) override
  {
    interface.use(AttributeType::Float3, "Position");
    interface.use(AttributeType::Float3, "Velocity");
    interface.use(AttributeType::Float, "Size");
  }
};

class ModifierStepDescription : public StepDescription {
 public:
  float m_duration;
  SmallMap<std::string, ModifierParticleType *> m_types;
  SmallVector<Emitter *> m_emitters;
  SmallVector<std::string> m_particle_type_names;

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

  ArrayRef<std::string> particle_type_names() override
  {
    return m_particle_type_names;
  }

  ParticleType &particle_type(StringRef type_name) override
  {
    return *m_types.lookup(type_name.to_std_string());
  }
};

}  // namespace BParticles
