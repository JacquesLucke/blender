#pragma once

#include "particles_container.hpp"
#include "BLI_string_map.hpp"

namespace BParticles {

using BLI::StringMap;

class ParticlesState {
 private:
  StringMap<ParticlesContainer *> m_container_by_id;

 public:
  ParticlesState() = default;
  ParticlesState(ParticlesState &other) = delete;
  ~ParticlesState();

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
  return *m_container_by_id.lookup(name);
}

inline StringRefNull ParticlesState::particle_container_name(ParticlesContainer &container)
{
  StringRefNull result = m_container_by_id.find_key_for_value(&container);
  return result;
}

}  // namespace BParticles
