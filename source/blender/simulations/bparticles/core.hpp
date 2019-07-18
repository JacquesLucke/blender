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
#include "time_span.hpp"

namespace BParticles {

using BLI::StringMap;

class EventFilterInterface;
class EventExecuteInterface;
class EmitterInterface;
class IntegratorInterface;
class ForwardingListenerInterface;

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
  virtual void attributes(AttributesInfoBuilder &interface);
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

class ForwardingListener {
 public:
  virtual ~ForwardingListener();

  virtual void listen(ForwardingListenerInterface &interface) = 0;
};

/**
 * Describes how one type of particle behaves and which attributes it has.
 */
class ParticleType {
 public:
  virtual ~ParticleType();

  /**
   * Return the integrator to be used with particles of this type.
   */
  virtual Integrator &integrator() = 0;

  virtual ArrayRef<ForwardingListener *> forwarding_listeners();

  /**
   * Return the events that particles of this type can trigger.
   */
  virtual ArrayRef<Event *> events();

  /**
   * Allows to define which attributes should exist for the type.
   */
  virtual void attributes(AttributesInfoBuilder &interface);
};

/**
 * Describes how the current state of a particle system transitions to the next state.
 */
class StepDescription {
 public:
  virtual ~StepDescription();

  /**
   * Return how many seconds the this time step takes.
   */
  virtual float step_duration() = 0;

  /**
   * Return the emitters that might emit particles in this time step.
   */
  virtual ArrayRef<Emitter *> emitters() = 0;

  /**
   * Return the particle type ids that will be modified in this step.
   */
  virtual ArrayRef<std::string> particle_type_names() = 0;

  /**
   * Return the description of a particle type based on its id.
   */
  virtual ParticleType &particle_type(StringRef name) = 0;
};

/* Classes used by the interface
 ***********************************************/

/**
 * This holds the current state of an entire particle particle system. It only knows about the
 * particles and the current time, not how the system got there.
 *
 * The state can also be created independent of any particle system. It gets "fixed up" when it is
 * used in a simulation.
 */
class ParticlesState {
 private:
  StringMap<ParticlesContainer *> m_container_by_id;
  float m_current_time = 0.0f;
  uint m_current_step = 0;

 public:
  ParticlesState() = default;
  ParticlesState(ParticlesState &other) = delete;
  ~ParticlesState();

  /**
   * Access the time since the simulation started.
   */
  float current_time() const;

  /**
   * Move current time forward.
   */
  void increase_time(float time_step);

  /**
   * Get the current simulation step.
   */
  uint current_step() const;

  /**
   * Access the mapping from particle type names to their corresponding containers.
   */
  StringMap<ParticlesContainer *> &particle_containers();

  /**
   * Get the container corresponding to a particle type name.
   * Asserts when the container does not exist.
   */
  ParticlesContainer &particle_container(StringRef name);

  /**
   * Get the name of a container in the context of this particle state.
   */
  StringRefNull particle_container_id(ParticlesContainer &container);
};

/**
 * A set of particles all of which are in the same block.
 */
struct ParticleSet {
 private:
  ParticlesBlock *m_block;

  /* Indices into the attribute arrays.
   * Invariants:
   *   - Every index must exist at most once.
   *   - The indices must be sorted. */
  ArrayRef<uint> m_pindices;

 public:
  ParticleSet(ParticlesBlock &block, ArrayRef<uint> pindices);

  /**
   * Return the block that contains the particles of this set.
   */
  ParticlesBlock &block();

  /**
   * Access the attributes of particles in the block on this set.
   */
  AttributeArrays attributes();

  /**
   * Access particle indices in the block that are part of the set.
   * Every value in this array is an index into the attribute arrays.
   */
  ArrayRef<uint> pindices();

  /**
   * Number of particles in this set.
   */
  uint size();

  /**
   * Returns true when pindices()[i] == i for all i, otherwise false.
   */
  bool indices_are_trivial();
};

class ParticleSets {
 private:
  std::string m_particle_type_name;
  AttributesInfo &m_attributes_info;
  SmallVector<ParticleSet> m_sets;
  uint m_size;

 public:
  ParticleSets(StringRef particle_type_name,
               AttributesInfo &attributes_info,
               ArrayRef<ParticleSet> sets);

  ArrayRef<ParticleSet> sets();

  void set_byte(uint index, ArrayRef<uint8_t> data);
  void set_byte(StringRef name, ArrayRef<uint8_t> data);
  void set_float(uint index, ArrayRef<float> data);
  void set_float(StringRef name, ArrayRef<float> data);
  void set_float3(uint index, ArrayRef<float3> data);
  void set_float3(StringRef name, ArrayRef<float3> data);

  void set_repeated_byte(uint index, ArrayRef<uint8_t> data);
  void set_repeated_byte(StringRef name, ArrayRef<uint8_t> data);
  void set_repeated_float(uint index, ArrayRef<float> data);
  void set_repeated_float(StringRef name, ArrayRef<float> data);
  void set_repeated_float3(uint index, ArrayRef<float3> data);
  void set_repeated_float3(StringRef name, ArrayRef<float3> data);

  void fill_byte(uint index, uint8_t value);
  void fill_byte(StringRef name, uint8_t value);
  void fill_float(uint index, float value);
  void fill_float(StringRef name, float value);
  void fill_float3(uint index, float3 value);
  void fill_float3(StringRef name, float3 value);

  StringRefNull particle_type_name();

  AttributesInfo &attributes_info();

 private:
  void set_elements(uint index, void *data);
  void set_repeated_elements(uint index,
                             void *data,
                             uint data_element_amount,
                             void *default_value);
  void fill_elements(uint index, void *value);
};

/**
 * This class allows allocating new blocks from different particle containers.
 * A single instance is not thread safe, but multiple allocator instances can
 * be used by multiple threads at the same time.
 * It might hand out the same block more than once until it is full.
 */
class ParticleAllocator {
 private:
  ParticlesState &m_state;
  SmallVector<ParticlesBlock *> m_non_full_cache;
  SmallVector<ParticlesBlock *> m_allocated_blocks;

 public:
  ParticleAllocator(ParticlesState &state);
  ParticleAllocator(ParticleAllocator &other) = delete;
  ParticleAllocator(ParticleAllocator &&other) = delete;

  /**
   * Access all blocks that have been allocated by this allocator.
   */
  ArrayRef<ParticlesBlock *> allocated_blocks();

  ParticleSets request(StringRef particle_type_name, uint size);

  ParticlesState &particles_state();

 private:
  /**
   * Return a block that can hold new particles. It might create an entirely new one or use a
   * cached block.
   */
  ParticlesBlock &get_non_full_block(StringRef particle_type_name);

  /**
   * Allocate space for a given number of new particles. The attribute buffers might be distributed
   * over multiple blocks.
   */
  void allocate_block_ranges(StringRef particle_type_name,
                             uint size,
                             SmallVector<ParticlesBlock *> &r_blocks,
                             SmallVector<Range<uint>> &r_ranges);

  AttributesInfo &attributes_info(StringRef particle_type_name);
};

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

class ForwardingListenerInterface {
 private:
  BlockStepData &m_step_data;
  ArrayRef<uint> m_pindices;
  ArrayRef<float> m_time_factors;

 public:
  ForwardingListenerInterface(BlockStepData &step_data,
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

/* Event inline functions
 ********************************************/

inline void Event::attributes(AttributesInfoBuilder &UNUSED(builder))
{
}

/* ParticleType inline functions
 ********************************************/

inline void ParticleType::attributes(AttributesInfoBuilder &UNUSED(builder))
{
}

/* ParticlesState inline functions
 ********************************************/

inline StringMap<ParticlesContainer *> &ParticlesState::particle_containers()
{
  return m_container_by_id;
}

inline ParticlesContainer &ParticlesState::particle_container(StringRef name)
{
  return *m_container_by_id.lookup(name.to_std_string());
}

inline StringRefNull ParticlesState::particle_container_id(ParticlesContainer &container)
{
  for (auto item : m_container_by_id.items()) {
    if (item.value == &container) {
      return item.key;
    }
  }
  BLI_assert(false);
  return *(StringRefNull *)nullptr;
}

inline float ParticlesState::current_time() const
{
  return m_current_time;
}

inline void ParticlesState::increase_time(float time_step)
{
  BLI_assert(time_step >= 0.0f);
  m_current_time += time_step;
  m_current_step++;
}

inline uint ParticlesState::current_step() const
{
  return m_current_step;
}

/* ParticleAllocator inline functions
 ********************************************/

inline ParticlesState &ParticleAllocator::particles_state()
{
  return m_state;
}

inline ArrayRef<ParticlesBlock *> ParticleAllocator::allocated_blocks()
{
  return m_allocated_blocks;
}

/* ParticleSets inline functions
 ********************************************/

inline ArrayRef<ParticleSet> ParticleSets::sets()
{
  return m_sets;
}

inline StringRefNull ParticleSets::particle_type_name()
{
  return m_particle_type_name;
}

inline AttributesInfo &ParticleSets::attributes_info()
{
  return m_attributes_info;
}

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

/* ParticleSet inline functions
 *******************************************/

inline ParticleSet::ParticleSet(ParticlesBlock &block, ArrayRef<uint> pindices)
    : m_block(&block), m_pindices(pindices)
{
}

inline ParticlesBlock &ParticleSet::block()
{
  return *m_block;
}

inline AttributeArrays ParticleSet::attributes()
{
  return m_block->attributes();
}

inline ArrayRef<uint> ParticleSet::pindices()
{
  return m_pindices;
}

inline uint ParticleSet::size()
{
  return m_pindices.size();
}

inline bool ParticleSet::indices_are_trivial()
{
  if (m_pindices.size() == 0) {
    return true;
  }
  else {
    /* This works due to the invariants mentioned above. */
    return m_pindices.first() == 0 && m_pindices.last() == m_pindices.size() - 1;
  }
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

/* ForwardingListenerInterface inline functions
 **********************************************/

inline ParticleSet ForwardingListenerInterface::particles()
{
  return ParticleSet(m_step_data.block, m_pindices);
}

inline ParticleAllocator &ForwardingListenerInterface::particle_allocator()
{
  return m_step_data.particle_allocator;
}

inline AttributeArrays &ForwardingListenerInterface::offsets()
{
  return m_step_data.attribute_offsets;
}

inline ArrayRef<float> ForwardingListenerInterface::time_factors()
{
  return m_time_factors;
}

inline float ForwardingListenerInterface::step_end_time()
{
  return m_step_data.step_end_time;
}

inline ArrayRef<float> ForwardingListenerInterface::durations()
{
  return m_step_data.remaining_durations;
}

inline TimeSpan ForwardingListenerInterface::time_span(uint pindex)
{
  float duration = m_step_data.remaining_durations[pindex];
  return TimeSpan(m_step_data.step_end_time - duration, duration);
}

}  // namespace BParticles
