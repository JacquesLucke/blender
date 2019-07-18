#pragma once

#include <memory>
#include <functional>

#include "BLI_array_ref.hpp"
#include "BLI_small_set_vector.hpp"
#include "BLI_math.hpp"
#include "BLI_utildefines.h"
#include "BLI_string_ref.hpp"
#include "BLI_small_map.hpp"
#include "BLI_vector_adaptor.hpp"
#include "BLI_string_map.hpp"
#include "BLI_lazy_init.hpp"

#include "attributes.hpp"
#include "particles_container.hpp"
#include "particle_set.hpp"
#include "time_span.hpp"
#include "particle_allocator.hpp"
#include "particles_state.hpp"

namespace BParticles {

class EventFilterInterface;
class EventExecuteInterface;
class EmitterInterface;
class IntegratorInterface;
class OffsetHandlerInterface;

/* Main API for the particle simulation. These classes have to be subclassed to define how the
 * particles should behave.
 ******************************************/

/**
 * An event consists of two parts.
 *   1. Filter the particles that trigger the event within a specific time span.
 *   2. Modify the particles that were triggered.
 *
 * In some cases it is necessary to pass data from the filter to the execute function (e.g. the
 * normal of the surface at a collision point). So that is supported as well. Currently, only POD
 * (plain-old-data / simple C structs) can be used.
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

/* Classes used by the interface
 ***********************************************/

/**
 * The interface between the simulation core and individual emitters.
 */
class EmitterInterface {
 private:
  ParticleAllocator &m_particle_allocator;
  ArrayAllocator &m_array_allocator;
  TimeSpan m_time_span;

 public:
  EmitterInterface(ParticleAllocator &particle_allocator,
                   ArrayAllocator &array_allocator,
                   TimeSpan time_span);
  ~EmitterInterface() = default;

  ParticleAllocator &particle_allocator();
  ArrayAllocator &array_allocator();

  /**
   * Time span that new particles should be emitted in.
   */
  TimeSpan time_span();

  /**
   * True when this is the first time step in a simulation, otherwise false.
   */
  bool is_first_step();
};

struct BlockStepData {
  ArrayAllocator &array_allocator;
  ParticleAllocator &particle_allocator;
  ParticlesBlock &block;
  ParticleType &particle_type;
  AttributeArrays attribute_offsets;
  ArrayRef<float> remaining_durations;
  float step_end_time;
};

/**
 * Utility array wrapper that can hold different kinds of plain-old-data values.
 */
class EventStorage {
 private:
  void *m_array;
  uint m_stride;

 public:
  EventStorage(void *array, uint stride);
  EventStorage(EventStorage &other) = delete;

  void *operator[](uint index);
  template<typename T> T &get(uint index);

  uint max_element_size() const;
};

/**
 * Interface between the Event->filter() function and the core simulation code.
 */
class EventFilterInterface {
 private:
  BlockStepData &m_step_data;
  ArrayRef<uint> m_pindices;
  ArrayRef<float> m_known_min_time_factors;

  EventStorage &m_event_storage;
  SmallVector<uint> &m_filtered_pindices;
  SmallVector<float> &m_filtered_time_factors;

  /* Size can be increased when necessary. */
  char m_dummy_event_storage[64];

 public:
  EventFilterInterface(BlockStepData &step_data,
                       ArrayRef<uint> pindices,
                       ArrayRef<float> known_min_time_factors,
                       EventStorage &r_event_storage,
                       SmallVector<uint> &r_filtered_pindices,
                       SmallVector<float> &r_filtered_time_factors);

  /**
   * Return the particle set that should be checked.
   */
  ParticleSet particles();

  /**
   * Return the durations that should be checked for every particle.
   */
  ArrayRef<float> durations();

  /**
   * Return the offsets that every particle will experience when no event is triggered.
   */
  AttributeArrays attribute_offsets();

  /**
   * Get the time span that should be checked for a specific particle.
   */
  TimeSpan time_span(uint pindex);

  /**
   * Get the end time of the current time step.
   */
  float end_time();

  /**
   * Mark a particle as triggered by the event at a specific point in time.
   * Note: The index must increase between consecutive calls to this function.
   */
  void trigger_particle(uint pindex, float time_factor);

  /**
   * Same as above but returns a reference to a struct that can be used to pass data to the execute
   * function. The reference might point to a dummy buffer when the time_factor is after a known
   * other event.
   */
  template<typename T> T &trigger_particle(uint pindex, float time_factor);
};

/**
 * Interface between the Event->execute() function and the core simulation code.
 */
class EventExecuteInterface {
 private:
  BlockStepData &m_step_data;
  ArrayRef<uint> m_pindices;
  ArrayRef<float> m_current_times;
  EventStorage &m_event_storage;

 public:
  EventExecuteInterface(BlockStepData &step_data,
                        ArrayRef<uint> pindices,
                        ArrayRef<float> current_times,
                        EventStorage &event_storage);

  ~EventExecuteInterface() = default;

  /**
   * Access the set of particles that should be modified by this event.
   */
  ParticleSet particles();

  /**
   * Get the time at which every particle is modified by this event.
   */
  ArrayRef<float> current_times();

  ArrayRef<float> remaining_durations();

  /**
   * Get the data stored in the Event->filter() function for a particle index.
   */
  template<typename T> T &get_storage(uint pindex);

  /**
   * Access the offsets that are applied to every particle in the remaining time step.
   * The event is allowed to modify the arrays.
   */
  AttributeArrays attribute_offsets();

  /**
   * Get a block allocator. Not that the request_emit_target should usually be used instead.
   */
  ParticleAllocator &particle_allocator();

  ArrayAllocator &array_allocator();

  /**
   * Get the entire event storage.
   */
  EventStorage &event_storage();
};

/**
 * Interface between the Integrator->integrate() function and the core simulation code.
 */
class IntegratorInterface {
 private:
  ParticlesBlock &m_block;
  ArrayRef<float> m_durations;
  ArrayAllocator &m_array_allocator;

  AttributeArrays m_offsets;

 public:
  IntegratorInterface(ParticlesBlock &block,
                      ArrayRef<float> durations,
                      ArrayAllocator &array_allocator,
                      AttributeArrays r_offsets);

  /**
   * Get the block for which the attribute offsets should be computed.
   */
  ParticlesBlock &block();

  /**
   * Access durations for every particle that should be integrated.
   */
  ArrayRef<float> durations();

  /**
   * Get an array allocator that creates arrays with the number of elements being >= the number of
   * particles in the block.
   */
  ArrayAllocator &array_allocator()
  {
    return m_array_allocator;
  }

  /**
   * Get the arrays that the offsets should be written into.
   */
  AttributeArrays offset_targets();
};

class OffsetHandlerInterface {
 private:
  BlockStepData &m_step_data;
  ArrayRef<uint> m_pindices;
  ArrayRef<float> m_time_factors;

 public:
  OffsetHandlerInterface(BlockStepData &step_data,
                         ArrayRef<uint> pindices,
                         ArrayRef<float> time_factors);

  ParticleSet particles();
  ParticleAllocator &particle_allocator();
  AttributeArrays &offsets();
  ArrayRef<float> time_factors();
  float step_end_time();
  ArrayRef<float> durations();
  TimeSpan time_span(uint pindex);
};

/* EmitterInterface inline functions
 ***********************************************/

inline ParticleAllocator &EmitterInterface::particle_allocator()
{
  return m_particle_allocator;
}

inline ArrayAllocator &EmitterInterface::array_allocator()
{
  return m_array_allocator;
}

inline TimeSpan EmitterInterface::time_span()
{
  return m_time_span;
}

inline bool EmitterInterface::is_first_step()
{
  return m_particle_allocator.particles_state().current_step() == 1;
}

/* EventStorage inline functions
 ****************************************/

inline EventStorage::EventStorage(void *array, uint stride) : m_array(array), m_stride(stride)
{
}

inline void *EventStorage::operator[](uint index)
{
  return POINTER_OFFSET(m_array, m_stride * index);
}

template<typename T> inline T &EventStorage::get(uint index)
{
  return *(T *)(*this)[index];
}

inline uint EventStorage::max_element_size() const
{
  return m_stride;
}

/* EventFilterInterface inline functions
 **********************************************/

inline ParticleSet EventFilterInterface::particles()
{
  return ParticleSet(m_step_data.block, m_pindices);
}

inline ArrayRef<float> EventFilterInterface::durations()
{
  return m_step_data.remaining_durations;
}

inline TimeSpan EventFilterInterface::time_span(uint pindex)
{
  float duration = m_step_data.remaining_durations[pindex];
  return TimeSpan(m_step_data.step_end_time - duration, duration);
}

inline AttributeArrays EventFilterInterface::attribute_offsets()
{
  return m_step_data.attribute_offsets;
}

inline float EventFilterInterface::end_time()
{
  return m_step_data.step_end_time;
}

inline void EventFilterInterface::trigger_particle(uint pindex, float time_factor)
{
  BLI_assert(0.0f <= time_factor && time_factor <= 1.0f);

  if (time_factor <= m_known_min_time_factors[pindex]) {
    m_filtered_pindices.append(pindex);
    m_filtered_time_factors.append(time_factor);
  }
}

template<typename T>
inline T &EventFilterInterface::trigger_particle(uint pindex, float time_factor)
{
  BLI_STATIC_ASSERT(std::is_trivial<T>::value, "");
  BLI_assert(sizeof(T) <= m_event_storage.max_element_size());
  BLI_assert(sizeof(m_dummy_event_storage) >= m_event_storage.max_element_size());

  if (time_factor <= m_known_min_time_factors[pindex]) {
    this->trigger_particle(pindex, time_factor);
    return m_event_storage.get<T>(pindex);
  }
  else {
    return *(T *)m_dummy_event_storage;
  }
}

/* EventExecuteInterface inline functions
 **********************************************/

inline ParticleAllocator &EventExecuteInterface::particle_allocator()
{
  return m_step_data.particle_allocator;
}

inline ArrayAllocator &EventExecuteInterface::array_allocator()
{
  return m_step_data.array_allocator;
}

inline EventStorage &EventExecuteInterface::event_storage()
{
  return m_event_storage;
}

inline ParticleSet EventExecuteInterface::particles()
{
  return ParticleSet(m_step_data.block, m_pindices);
}

inline ArrayRef<float> EventExecuteInterface::current_times()
{
  return m_current_times;
}

inline ArrayRef<float> EventExecuteInterface::remaining_durations()
{
  return m_step_data.remaining_durations;
}

template<typename T> inline T &EventExecuteInterface::get_storage(uint pindex)
{
  BLI_STATIC_ASSERT(std::is_trivial<T>::value, "");
  BLI_assert(sizeof(T) <= m_event_storage.max_element_size());
  return m_event_storage.get<T>(pindex);
}

inline AttributeArrays EventExecuteInterface::attribute_offsets()
{
  return m_step_data.attribute_offsets;
}

/* IntegratorInterface inline functions
 *********************************************/

inline ParticlesBlock &IntegratorInterface::block()
{
  return m_block;
}

inline ArrayRef<float> IntegratorInterface::durations()
{
  return m_durations;
}

inline AttributeArrays IntegratorInterface::offset_targets()
{
  return m_offsets;
}

/* OffsetHandlerInterface inline functions
 **********************************************/

inline ParticleSet OffsetHandlerInterface::particles()
{
  return ParticleSet(m_step_data.block, m_pindices);
}

inline ParticleAllocator &OffsetHandlerInterface::particle_allocator()
{
  return m_step_data.particle_allocator;
}

inline AttributeArrays &OffsetHandlerInterface::offsets()
{
  return m_step_data.attribute_offsets;
}

inline ArrayRef<float> OffsetHandlerInterface::time_factors()
{
  return m_time_factors;
}

inline float OffsetHandlerInterface::step_end_time()
{
  return m_step_data.step_end_time;
}

inline ArrayRef<float> OffsetHandlerInterface::durations()
{
  return m_step_data.remaining_durations;
}

inline TimeSpan OffsetHandlerInterface::time_span(uint pindex)
{
  float duration = m_step_data.remaining_durations[pindex] * m_time_factors[pindex];
  return TimeSpan(m_step_data.step_end_time - duration, duration);
}

}  // namespace BParticles
