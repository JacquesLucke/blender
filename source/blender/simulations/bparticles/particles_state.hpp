#pragma once

#include "particles_container.hpp"
#include "BLI_string_map.hpp"

namespace BParticles {

using BLI::StringMap;

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
  StringRefNull particle_container_name(ParticlesContainer &container);
};

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

inline StringRefNull ParticlesState::particle_container_name(ParticlesContainer &container)
{
  StringRefNull result = m_container_by_id.find_key_for_value(&container);
  return result;
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

}  // namespace BParticles
