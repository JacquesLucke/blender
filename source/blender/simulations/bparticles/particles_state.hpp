#pragma once

#include <atomic>

#include "particle_set.hpp"

namespace BParticles {

using BLI::ArrayRef;
using BLI::IndexRange;
using BLI::Map;
using BLI::MutableArrayRef;
using BLI::StringMap;
using BLI::StringRef;
using BLI::StringRefNull;
using BLI::Vector;
using BLI::VectorSet;
using FN::AttributesInfo;
using FN::AttributesRef;
using FN::MutableAttributesRef;

class ParticlesState {
 private:
  StringMap<ParticleSet *> m_container_by_id;
  std::atomic<uint> m_next_id;

 public:
  ParticlesState() : m_next_id(0)
  {
  }
  ParticlesState(ParticlesState &other) = delete;
  ~ParticlesState();

  /**
   * Access the mapping from particle system names to their corresponding containers.
   */
  StringMap<ParticleSet *> &particle_containers();

  /**
   * Get the container corresponding to a particle system name.
   * Asserts when the container does not exist.
   */
  ParticleSet &particle_container(StringRef name);

  /**
   * Get the name of a container in the context of this particle state.
   */
  StringRefNull particle_container_name(ParticleSet &container);

  /**
   * Get range of unique particle ids.
   */
  IndexRange get_new_particle_ids(uint amount)
  {
    uint start = m_next_id.fetch_add(amount);
    return IndexRange(start, amount);
  }
};

/* ParticlesState inline functions
 ********************************************/

inline StringMap<ParticleSet *> &ParticlesState::particle_containers()
{
  return m_container_by_id;
}

inline ParticleSet &ParticlesState::particle_container(StringRef name)
{
  return *m_container_by_id.lookup(name);
}

inline StringRefNull ParticlesState::particle_container_name(ParticleSet &container)
{
  StringRefNull result = m_container_by_id.find_key_for_value(&container);
  return result;
}

}  // namespace BParticles
