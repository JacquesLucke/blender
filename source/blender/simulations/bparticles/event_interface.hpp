#pragma once

#include "block_step_data.hpp"
#include "particle_allocator.hpp"

namespace BParticles {

/**
 * Interface between the Event->filter() function and the core simulation code.
 */
class EventFilterInterface : public BlockStepDataAccess {
 private:
  ArrayRef<uint> m_pindices;
  ArrayRef<float> m_known_min_time_factors;

  Vector<uint> &m_filtered_pindices;
  Vector<float> &m_filtered_time_factors;

  /* Size can be increased when necessary. */
  char m_dummy_event_storage[64];

 public:
  EventFilterInterface(BlockStepData &step_data,
                       ArrayRef<uint> pindices,
                       ArrayRef<float> known_min_time_factors,
                       Vector<uint> &r_filtered_pindices,
                       Vector<float> &r_filtered_time_factors)
      : BlockStepDataAccess(step_data),
        m_pindices(pindices),
        m_known_min_time_factors(known_min_time_factors),
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
};

/**
 * Interface between the Event->execute() function and the core simulation code.
 */
class EventExecuteInterface : public BlockStepDataAccess {
 private:
  ArrayRef<uint> m_pindices;
  ArrayRef<float> m_current_times;
  ParticleAllocator &m_particle_allocator;

 public:
  EventExecuteInterface(BlockStepData &step_data,
                        ArrayRef<uint> pindices,
                        ArrayRef<float> current_times,
                        ParticleAllocator &particle_allocator)
      : BlockStepDataAccess(step_data),
        m_pindices(pindices),
        m_current_times(current_times),
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

}  // namespace BParticles
