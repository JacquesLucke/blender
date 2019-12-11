#pragma once

#include "particle_allocator.hpp"
#include "simulation_state.hpp"
#include "time_span.hpp"

namespace BParticles {

class EmitterInterface {
 private:
  SimulationState &m_simulation_state;
  ParticleAllocator &m_particle_allocator;
  TimeSpan m_time_span;

 public:
  EmitterInterface(SimulationState &simulation_state,
                   ParticleAllocator &particle_allocator,
                   TimeSpan time_span)
      : m_simulation_state(simulation_state),
        m_particle_allocator(particle_allocator),
        m_time_span(time_span)
  {
  }

  ~EmitterInterface() = default;

  ParticleAllocator &particle_allocator()
  {
    return m_particle_allocator;
  }

  /**
   * Time span that new particles should be emitted in.
   */
  TimeSpan time_span()
  {
    return m_time_span;
  }

  uint time_step()
  {
    return m_simulation_state.time().current_update_index();
  }

  /**
   * True when this is the first time step in a simulation, otherwise false.
   */
  bool is_first_step()
  {
    return m_simulation_state.time().current_update_index() == 1;
  }
};

/**
 * An emitter creates new particles of possibly different types within a certain time span.
 */
class Emitter {
 public:
  virtual ~Emitter()
  {
  }

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

}  // namespace BParticles
