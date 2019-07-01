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

#include "attributes.hpp"
#include "particles_container.hpp"
#include "time_span.hpp"

namespace BParticles {

class EventFilterInterface;
class EventExecuteInterface;
class EmitterInterface;

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
  virtual void integrate(ParticlesBlock &block,
                         ArrayRef<float> durations,
                         AttributeArrays r_offsets) = 0;
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

  /**
   * Return the events that particles of this type can trigger.
   */
  virtual ArrayRef<Event *> events() = 0;

  virtual ArrayRef<std::string> byte_attributes()
  {
    return {};
  }

  virtual ArrayRef<std::string> float_attributes()
  {
    return {};
  }

  virtual ArrayRef<std::string> float3_attributes()
  {
    return {};
  }
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
  virtual ArrayRef<uint> particle_type_ids() = 0;

  /**
   * Return the description of a particle type based on its id.
   */
  virtual ParticleType &particle_type(uint type_id) = 0;
};

/* Classes used by the interface
 ***********************************************/

class ParticlesState {
 private:
  SmallMap<uint, ParticlesContainer *> m_container_by_id;

 public:
  float m_current_time = 0.0f;

  ParticlesState() = default;
  ParticlesState(ParticlesState &other) = delete;
  ~ParticlesState();

  SmallMap<uint, ParticlesContainer *> &particle_containers();

  ParticlesContainer &particle_container(uint type_id);
  uint particle_container_id(ParticlesContainer &container);
};

/**
 * This class allows allocating new blocks from different particle containers.
 * A single instance is not thread safe, but multiple allocator instances can
 * be used by multiple threads at the same time.
 * It might hand out the same block more than once until it is full.
 */
class BlockAllocator {
 private:
  ParticlesState &m_state;
  SmallVector<ParticlesBlock *> m_non_full_cache;
  SmallVector<ParticlesBlock *> m_allocated_blocks;

 public:
  BlockAllocator(ParticlesState &state);
  BlockAllocator(BlockAllocator &other) = delete;

  ParticlesBlock &get_non_full_block(uint particle_type_id);
  void allocate_block_ranges(uint particle_type_id,
                             uint size,
                             SmallVector<ParticlesBlock *> &r_blocks,
                             SmallVector<Range<uint>> &r_ranges);

  AttributesInfo &attributes_info(uint particle_type_id);
  ParticlesState &particles_state();
  ArrayRef<ParticlesBlock *> allocated_blocks();
};

class EmitTargetBase {
 protected:
  uint m_particle_type_id;
  AttributesInfo &m_attributes_info;
  SmallVector<ParticlesBlock *> m_blocks;
  SmallVector<Range<uint>> m_ranges;

  uint m_size = 0;

 public:
  EmitTargetBase(uint particle_type_id,
                 AttributesInfo &attributes_info,
                 ArrayRef<ParticlesBlock *> blocks,
                 ArrayRef<Range<uint>> ranges);

  EmitTargetBase(EmitTargetBase &other) = delete;

  void set_byte(uint index, ArrayRef<uint8_t> data);
  void set_byte(StringRef name, ArrayRef<uint8_t> data);
  void set_float(uint index, ArrayRef<float> data);
  void set_float(StringRef name, ArrayRef<float> data);
  void set_float3(uint index, ArrayRef<float3> data);
  void set_float3(StringRef name, ArrayRef<float3> data);

  void fill_byte(uint index, uint8_t value);
  void fill_byte(StringRef name, uint8_t value);
  void fill_float(uint index, float value);
  void fill_float(StringRef name, float value);
  void fill_float3(uint index, float3 value);
  void fill_float3(StringRef name, float3 value);

  ArrayRef<ParticlesBlock *> blocks();
  ArrayRef<Range<uint>> ranges();
  uint part_amount();
  AttributeArrays attributes(uint part);
  uint particle_type_id();

 private:
  void set_elements(uint index, void *data);
  void fill_elements(uint index, void *value);
};

class InstantEmitTarget : public EmitTargetBase {
 public:
  InstantEmitTarget(uint particle_type_id,
                    AttributesInfo &attributes_info,
                    ArrayRef<ParticlesBlock *> blocks,
                    ArrayRef<Range<uint>> ranges);
};

class TimeSpanEmitTarget : public EmitTargetBase {
 private:
  TimeSpan m_time_span;

 public:
  TimeSpanEmitTarget(uint particle_type_id,
                     AttributesInfo &attributes_info,
                     ArrayRef<ParticlesBlock *> blocks,
                     ArrayRef<Range<uint>> ranges,
                     TimeSpan time_span);

  void set_birth_moment(float time_factor);
  void set_randomized_birth_moments();
};

class EmitterInterface {
 private:
  BlockAllocator &m_block_allocator;
  SmallVector<TimeSpanEmitTarget *> m_targets;
  TimeSpan m_time_span;

 public:
  EmitterInterface(BlockAllocator &allocator, TimeSpan time_span);
  ~EmitterInterface();

  ArrayRef<TimeSpanEmitTarget *> targets();

  TimeSpanEmitTarget &request(uint particle_type_id, uint size);
  TimeSpan time_span();
  bool is_first_step();
};

struct ParticleSet {
 private:
  ParticlesBlock *m_block;

  /* Indices into the attribute arrays.
   * Invariants:
   *   - Every index must exist at most once.
   *   - The indices must be sorted. */
  ArrayRef<uint> m_particle_indices;

 public:
  ParticleSet(ParticlesBlock &block, ArrayRef<uint> particle_indices);

  ParticlesBlock &block();
  AttributeArrays attributes();
  ArrayRef<uint> indices();
  uint get_particle_index(uint i);
  Range<uint> range();
  uint size();

  /**
   * Returns true when get_particle_index(i) == i for all i, otherwise false.
   */
  bool indices_are_trivial();
};

class EventStorage {
 private:
  void *m_array;
  uint m_stride;

 public:
  EventStorage(void *array, uint stride);
  EventStorage(EventStorage &other) = delete;

  void *operator[](uint index);
  template<typename T> T &get(uint index);
};

class EventFilterInterface {
 private:
  ParticleSet m_particles;
  AttributeArrays &m_attribute_offsets;
  ArrayRef<float> m_durations;
  float m_end_time;

  EventStorage &m_event_storage;
  SmallVector<uint> &m_filtered_indices;
  SmallVector<float> &m_filtered_time_factors;

 public:
  EventFilterInterface(ParticleSet particles,
                       AttributeArrays &attribute_offsets,
                       ArrayRef<float> durations,
                       float end_time,
                       EventStorage &r_event_storage,
                       SmallVector<uint> &r_filtered_indices,
                       SmallVector<float> &r_filtered_time_factors);

  ParticleSet &particles();
  ArrayRef<float> durations();
  TimeSpan time_span(uint index);
  AttributeArrays attribute_offsets();
  float end_time();

  void trigger_particle(uint index, float time_factor);
  template<typename T> T &trigger_particle(uint index, float time_factor);
};

class EventExecuteInterface {
 private:
  ParticleSet m_particles;
  BlockAllocator &m_block_allocator;
  SmallVector<InstantEmitTarget *> m_emit_targets;
  ArrayRef<float> m_current_times;
  ArrayRef<uint8_t> m_kill_states;
  EventStorage &m_event_storage;
  AttributeArrays m_attribute_offsets;

 public:
  EventExecuteInterface(ParticleSet particles,
                        BlockAllocator &block_allocator,
                        ArrayRef<float> current_times,
                        EventStorage &event_storage,
                        AttributeArrays attribute_offsets);

  ~EventExecuteInterface();

  InstantEmitTarget &request_emit_target(uint particle_type_id, ArrayRef<uint> original_indices);
  void kill(ArrayRef<uint> particle_indices);

  BlockAllocator &block_allocator();
  ParticleSet &particles();
  ArrayRef<InstantEmitTarget *> emit_targets();
  ArrayRef<float> current_times();
  template<typename T> T &get_storage(uint pindex);
  AttributeArrays attribute_offsets();
};

/* ParticlesState inline functions
 ********************************************/

inline SmallMap<uint, ParticlesContainer *> &ParticlesState::particle_containers()
{
  return m_container_by_id;
}

inline ParticlesContainer &ParticlesState::particle_container(uint type_id)
{
  return *m_container_by_id.lookup(type_id);
}

inline uint ParticlesState::particle_container_id(ParticlesContainer &container)
{
  for (auto item : m_container_by_id.items()) {
    if (item.value == &container) {
      return item.key;
    }
  }
  BLI_assert(false);
  return 0;
}

/* BlockAllocator inline functions
 ********************************************/

inline ParticlesState &BlockAllocator::particles_state()
{
  return m_state;
}

inline ArrayRef<ParticlesBlock *> BlockAllocator::allocated_blocks()
{
  return m_allocated_blocks;
}

/* EmitTargetBase inline functions
 ********************************************/

inline ArrayRef<ParticlesBlock *> EmitTargetBase::blocks()
{
  return m_blocks;
}

inline ArrayRef<Range<uint>> EmitTargetBase::ranges()
{
  return m_ranges;
}

inline uint EmitTargetBase::part_amount()
{
  return m_ranges.size();
}

inline AttributeArrays EmitTargetBase::attributes(uint part)
{
  return m_blocks[part]->slice(m_ranges[part]);
}

inline uint EmitTargetBase::particle_type_id()
{
  return m_particle_type_id;
}

/* EmitterInterface inline functions
 ***********************************************/

inline ArrayRef<TimeSpanEmitTarget *> EmitterInterface::targets()
{
  return m_targets;
}

inline TimeSpan EmitterInterface::time_span()
{
  return m_time_span;
}

inline bool EmitterInterface::is_first_step()
{
  return m_time_span.start() < 0.00001f;
}

/* ParticleSet inline functions
 *******************************************/

inline ParticleSet::ParticleSet(ParticlesBlock &block, ArrayRef<uint> particle_indices)
    : m_block(&block), m_particle_indices(particle_indices)
{
}

inline ParticlesBlock &ParticleSet::block()
{
  return *m_block;
}

inline AttributeArrays ParticleSet::attributes()
{
  return m_block->slice_all();
}

inline ArrayRef<uint> ParticleSet::indices()
{
  return m_particle_indices;
}

inline uint ParticleSet::get_particle_index(uint i)
{
  return m_particle_indices[i];
}

inline Range<uint> ParticleSet::range()
{
  return Range<uint>(0, m_particle_indices.size());
}

inline uint ParticleSet::size()
{
  return m_particle_indices.size();
}

inline bool ParticleSet::indices_are_trivial()
{
  if (m_particle_indices.size() == 0) {
    return true;
  }
  else {
    /* This works due to the invariants mentioned above. */
    return m_particle_indices.first() == 0 &&
           m_particle_indices.last() == m_particle_indices.size() - 1;
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

/* EventFilterInterface inline functions
 **********************************************/

inline ParticleSet &EventFilterInterface::particles()
{
  return m_particles;
}

inline ArrayRef<float> EventFilterInterface::durations()
{
  return m_durations;
}

inline TimeSpan EventFilterInterface::time_span(uint index)
{
  float duration = m_durations[index];
  return TimeSpan(m_end_time - duration, duration);
}

inline AttributeArrays EventFilterInterface::attribute_offsets()
{
  return m_attribute_offsets;
}

inline float EventFilterInterface::end_time()
{
  return m_end_time;
}

inline void EventFilterInterface::trigger_particle(uint index, float time_factor)
{
  m_filtered_indices.append(index);
  m_filtered_time_factors.append(time_factor);
}

template<typename T>
inline T &EventFilterInterface::trigger_particle(uint index, float time_factor)
{
  this->trigger_particle(index, time_factor);
  return m_event_storage.get<T>(m_particles.get_particle_index(index));
}

/* EventExecuteInterface inline functions
 **********************************************/

inline BlockAllocator &EventExecuteInterface::block_allocator()
{
  return m_block_allocator;
}

inline ParticleSet &EventExecuteInterface::particles()
{
  return m_particles;
}

inline void EventExecuteInterface::kill(ArrayRef<uint> particle_indices)
{
  for (uint pindex : particle_indices) {
    m_kill_states[pindex] = 1;
  }
}

inline ArrayRef<InstantEmitTarget *> EventExecuteInterface::emit_targets()
{
  return m_emit_targets;
}

inline ArrayRef<float> EventExecuteInterface::current_times()
{
  return m_current_times;
}

template<typename T> inline T &EventExecuteInterface::get_storage(uint pindex)
{
  return m_event_storage.get<T>(pindex);
}

inline AttributeArrays EventExecuteInterface::attribute_offsets()
{
  return m_attribute_offsets;
}

}  // namespace BParticles
