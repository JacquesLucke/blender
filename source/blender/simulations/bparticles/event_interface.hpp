#pragma once

#include "block_step_data.hpp"
#include "particle_allocator.hpp"

namespace BParticles {

using BKE::AttributesDeclaration;

/**
 * Utility array wrapper that can hold different kinds of plain-old-data values.
 */
class EventStorage {
 private:
  void *m_array;
  uint m_stride;

 public:
  EventStorage(void *array, uint stride) : m_array(array), m_stride(stride)
  {
  }

  EventStorage(EventStorage &other) = delete;

  void *operator[](uint index)
  {
    return POINTER_OFFSET(m_array, m_stride * index);
  }

  template<typename T> T &get(uint index)
  {
    return *(T *)(*this)[index];
  }

  uint max_element_size() const
  {
    return m_stride;
  }
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
                       Vector<float> &r_filtered_time_factors)
      : BlockStepDataAccess(step_data),
        m_pindices(pindices),
        m_known_min_time_factors(known_min_time_factors),
        m_event_storage(r_event_storage),
        m_filtered_pindices(r_filtered_pindices),
        m_filtered_time_factors(r_filtered_time_factors)
  {
  }

  /**
   * Return the indices that should be checked.
   */
  ArrayRef<uint> pindices()
  {
    return m_pindices;
  }

  /**
   * Mark a particle as triggered by the event at a specific point in time.
   * Note: The index must increase between consecutive calls to this function.
   */
  void trigger_particle(uint pindex, float time_factor)
  {
    BLI_assert(0.0f <= time_factor && time_factor <= 1.0f);

    if (time_factor <= m_known_min_time_factors[pindex]) {
      m_filtered_pindices.append(pindex);
      m_filtered_time_factors.append(time_factor);
    }
  }

  /**
   * Same as above but returns a reference to a struct that can be used to pass data to the execute
   * function. The reference might point to a dummy buffer when the time_factor is after a known
   * other event.
   */
  template<typename T> T &trigger_particle(uint pindex, float time_factor)
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
                        ParticleAllocator &particle_allocator)
      : BlockStepDataAccess(step_data),
        m_pindices(pindices),
        m_current_times(current_times),
        m_event_storage(event_storage),
        m_particle_allocator(particle_allocator)
  {
  }

  ~EventExecuteInterface() = default;

  /**
   * Access the indices that should be modified by this event.
   */
  ArrayRef<uint> pindices()
  {
    return m_pindices;
  }

  /**
   * Get the time at which every particle is modified by this event.
   */
  ArrayRef<float> current_times()
  {
    return m_current_times;
  }

  /**
   * Get the data stored in the Event->filter() function for a particle index.
   */
  template<typename T> T &get_storage(uint pindex)
  {
    BLI_STATIC_ASSERT(std::is_trivial<T>::value, "");
    BLI_assert(sizeof(T) <= m_event_storage.max_element_size());
    return m_event_storage.get<T>(pindex);
  }

  /**
   * Get the entire event storage.
   */
  EventStorage &event_storage()
  {
    return m_event_storage;
  }

  ParticleAllocator &particle_allocator()
  {
    return m_particle_allocator;
  }
};

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
  virtual ~Event()
  {
  }

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
  virtual void attributes(AttributesDeclaration &UNUSED(interface))
  {
  }
};

}  // namespace BParticles
