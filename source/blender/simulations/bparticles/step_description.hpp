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

  void attributes(AttributesInfoBuilder &builder) override
  {
    builder.use_float3("Position", {0, 0, 0});
    builder.use_float3("Velocity", {0, 0, 0});
    builder.use_float("Size", 0.01f);
  }
};

class ModifierStepDescription : public StepDescription {
 public:
  float m_duration;
  StringMap<ModifierParticleType *> m_types;
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
