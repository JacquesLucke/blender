#pragma once

#include "step_description_interfaces.hpp"

namespace BParticles {

/**
 * An event consists of two parts.
 *   1. Filter the particles that trigger the event within a specific time span.
 *   2. Modify the particles that were triggered.
 *
 * In some cases it is necessary to pass data from the filter to the execute function (e.g. the
 * normal of the surface at a collision point). So that is supported as well. Currently, only
 * POD (plain-old-data / simple C structs) can be used.
 */
class Event {
 public:
  virtual ~Event();

  /**
   * Return how many bytes this event wants to pass between the filter and execute function.
   */
  virtual uint storage_size()
  {
    return 0;
  }

  /**
   * Gets a set of particles and checks which of those trigger the event.
   */
  virtual void filter(EventFilterInterface &interface) = 0;

  /**
   * Gets a set of particles that trigger this event and can do the following operations:
   *   - Change any attribute of the particles.
   *   - Change the remaining integrated attribute offsets of the particles.
   *   - Kill the particles.
   *   - Spawn new particles of any type.
   *
   * Currently, it is not supported to change the attributes of other particles, that exist
   * already. However, the attributes of new particles can be changed.
   */
  virtual void execute(EventExecuteInterface &interface) = 0;

  /**
   * Allows to define which attributes are required by the event.
   */
  virtual void attributes(AttributesDeclaration &interface);
};

/**
 * An emitter creates new particles of possibly different types within a certain time span.
 */
class Emitter {
 public:
  virtual ~Emitter();

  /**
   * Create new particles within a time span.
   *
   * In general it works like so:
   *   1. Prepare vectors with attribute values for e.g. position and velocity of the new
   *      particles.
   *   2. Request an emit target that can contain a given amount of particles of a specific type.
   *   3. Copy the prepared attribute arrays into the target. Other attributes are initialized with
   *      some default value.
   *   4. Specify the exact birth times of every particle within the time span. This will allow the
   *      framework to simulate the new particles for partial time steps to avoid stepping.
   *
   * To create particles of different types, multiple emit targets have to be requested.
   */
  virtual void emit(EmitterInterface &interface) = 0;
};

/**
 * The integrator is the core of the particle system. It's main task is to determine how the
 * simulation would go if there were no events.
 */
class Integrator {
 public:
  virtual ~Integrator();

  /**
   * Specify which attributes are integrated (usually Position and Velocity).
   */
  virtual AttributesInfo &offset_attributes_info() = 0;

  /**
   * Compute the offsets for all integrated attributes. Those are not applied immediately, because
   * there might be events that modify the attributes within a time step.
   */
  virtual void integrate(IntegratorInterface &interface) = 0;
};

class OffsetHandler {
 public:
  virtual ~OffsetHandler();

  virtual void execute(OffsetHandlerInterface &interface) = 0;
};

/**
 * Describes how one type of particle behaves and which attributes it has.
 */
class ParticleType {
 private:
  AttributesDeclaration m_attributes;
  Integrator *m_integrator;
  SmallVector<Event *> m_events;
  SmallVector<OffsetHandler *> m_offset_handlers;

 public:
  ParticleType(AttributesDeclaration &attributes,
               Integrator *integrator,
               ArrayRef<Event *> events,
               ArrayRef<OffsetHandler *> offset_handlers)
      : m_attributes(attributes),
        m_integrator(integrator),
        m_events(events),
        m_offset_handlers(offset_handlers)
  {
  }

  ~ParticleType();

  Integrator &integrator()
  {
    return *m_integrator;
  }

  ArrayRef<OffsetHandler *> offset_handlers()
  {
    return m_offset_handlers;
  }

  ArrayRef<Event *> events()
  {
    return m_events;
  }

  AttributesDeclaration &attributes()
  {
    return m_attributes;
  }
};

/**
 * Describes how the current state of a particle system transitions to the next state.
 */
class StepDescription {
  float m_duration;
  StringMap<ParticleType *> m_types;
  SmallVector<Emitter *> m_emitters;

 public:
  StepDescription(float duration, StringMap<ParticleType *> types, ArrayRef<Emitter *> emitters)
      : m_duration(duration), m_types(types), m_emitters(emitters)
  {
  }

  ~StepDescription();

  float step_duration()
  {
    return m_duration;
  }

  ArrayRef<Emitter *> emitters()
  {
    return m_emitters;
  }

  StringMap<ParticleType *> &particle_types()
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
  ~StepDescriptionBuilder()
  {
    for (auto type : m_type_builders.values()) {
      delete type;
    }
  }

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
