#pragma once

#include "particle_allocator.hpp"
#include "time_span.hpp"
#include "world_state.hpp"

namespace BParticles {

struct BlockStepData {
  AttributeArrays attributes;
  AttributeArrays attribute_offsets;
  MutableArrayRef<float> remaining_durations;
  float step_end_time;

  uint array_size()
  {
    return this->remaining_durations.size();
  }
};

class BlockStepDataAccess {
 protected:
  BlockStepData &m_step_data;

 public:
  BlockStepDataAccess(BlockStepData &step_data) : m_step_data(step_data)
  {
  }

  uint array_size() const
  {
    return m_step_data.array_size();
  }

  BlockStepData &step_data()
  {
    return m_step_data;
  }

  AttributeArrays attributes()
  {
    return m_step_data.attributes;
  }

  AttributeArrays attribute_offsets()
  {
    return m_step_data.attribute_offsets;
  }

  MutableArrayRef<float> remaining_durations()
  {
    return m_step_data.remaining_durations;
  }

  float step_end_time()
  {
    return m_step_data.step_end_time;
  }

  TimeSpan time_span(uint pindex)
  {
    float duration = m_step_data.remaining_durations[pindex];
    return TimeSpan(m_step_data.step_end_time - duration, duration);
  }
};

/**
 * The interface between the simulation core and individual emitters.
 */
class EmitterInterface {
 private:
  ParticleAllocator &m_particle_allocator;
  TimeSpan m_time_span;
  WorldTransition &m_world_transition;

 public:
  EmitterInterface(ParticleAllocator &particle_allocator,
                   TimeSpan time_span,
                   WorldTransition &world_transition);
  ~EmitterInterface() = default;

  ParticleAllocator &particle_allocator();

  /**
   * Time span that new particles should be emitted in.
   */
  TimeSpan time_span();

  /**
   * True when this is the first time step in a simulation, otherwise false.
   */
  bool is_first_step();

  WorldTransition &world_transition();
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
class EventFilterInterface : public BlockStepDataAccess {
 private:
  ArrayRef<uint> m_pindices;
  ArrayRef<float> m_known_min_time_factors;

  EventStorage &m_event_storage;
  Vector<uint> &m_filtered_pindices;
  Vector<float> &m_filtered_time_factors;

  /* Size can be increased when necessary. */
  char m_dummy_event_storage[64];

 public:
  EventFilterInterface(BlockStepData &step_data,
                       ArrayRef<uint> pindices,
                       ArrayRef<float> known_min_time_factors,
                       EventStorage &r_event_storage,
                       Vector<uint> &r_filtered_pindices,
                       Vector<float> &r_filtered_time_factors);

  /**
   * Return the indices that should be checked.
   */
  ArrayRef<uint> pindices();

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
class EventExecuteInterface : public BlockStepDataAccess {
 private:
  ArrayRef<uint> m_pindices;
  ArrayRef<float> m_current_times;
  EventStorage &m_event_storage;
  ParticleAllocator &m_particle_allocator;

 public:
  EventExecuteInterface(BlockStepData &step_data,
                        ArrayRef<uint> pindices,
                        ArrayRef<float> current_times,
                        EventStorage &event_storage,
                        ParticleAllocator &particle_allocator);

  ~EventExecuteInterface() = default;

  /**
   * Access the indices that should be modified by this event.
   */
  ArrayRef<uint> pindices();

  /**
   * Get the time at which every particle is modified by this event.
   */
  ArrayRef<float> current_times();

  /**
   * Get the data stored in the Event->filter() function for a particle index.
   */
  template<typename T> T &get_storage(uint pindex);

  /**
   * Get the entire event storage.
   */
  EventStorage &event_storage();

  ParticleAllocator &particle_allocator();
};

/**
 * Interface between the Integrator->integrate() function and the core simulation code.
 */
class IntegratorInterface : public BlockStepDataAccess {
 private:
  ArrayRef<uint> m_pindices;

 public:
  IntegratorInterface(BlockStepData &step_data, ArrayRef<uint> pindices);

  ArrayRef<uint> pindices();
};

class OffsetHandlerInterface : public BlockStepDataAccess {
 private:
  ArrayRef<uint> m_pindices;
  ArrayRef<float> m_time_factors;
  ParticleAllocator &m_particle_allocator;

 public:
  OffsetHandlerInterface(BlockStepData &step_data,
                         ArrayRef<uint> pindices,
                         ArrayRef<float> time_factors,
                         ParticleAllocator &particle_allocator);

  ArrayRef<uint> pindices();
  ArrayRef<float> time_factors();
  ParticleAllocator &particle_allocator();
};

/* EmitterInterface inline functions
 ***********************************************/

inline ParticleAllocator &EmitterInterface::particle_allocator()
{
  return m_particle_allocator;
}

inline TimeSpan EmitterInterface::time_span()
{
  return m_time_span;
}

inline bool EmitterInterface::is_first_step()
{
  return m_time_span.start() == 0.0f;
}

inline WorldTransition &EmitterInterface::world_transition()
{
  return m_world_transition;
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

inline ArrayRef<uint> EventFilterInterface::pindices()
{
  return m_pindices;
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

inline EventStorage &EventExecuteInterface::event_storage()
{
  return m_event_storage;
}

inline ArrayRef<uint> EventExecuteInterface::pindices()
{
  return m_pindices;
}

inline ArrayRef<float> EventExecuteInterface::current_times()
{
  return m_current_times;
}

template<typename T> inline T &EventExecuteInterface::get_storage(uint pindex)
{
  BLI_STATIC_ASSERT(std::is_trivial<T>::value, "");
  BLI_assert(sizeof(T) <= m_event_storage.max_element_size());
  return m_event_storage.get<T>(pindex);
}

inline ParticleAllocator &EventExecuteInterface::particle_allocator()
{
  return m_particle_allocator;
}

/* OffsetHandlerInterface inline functions
 **********************************************/

inline ArrayRef<uint> OffsetHandlerInterface::pindices()
{
  return m_pindices;
}

inline ArrayRef<float> OffsetHandlerInterface::time_factors()
{
  return m_time_factors;
}

inline ParticleAllocator &OffsetHandlerInterface::particle_allocator()
{
  return m_particle_allocator;
}

/* IntegratorInterface inline functions
 **********************************************/

inline ArrayRef<uint> IntegratorInterface::pindices()
{
  return m_pindices;
}

}  // namespace BParticles
