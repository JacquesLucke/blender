#pragma once

#include <atomic>

#include "FN_attributes_block_container.h"

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
using FN::AttributesBlock;
using FN::AttributesBlockContainer;
using FN::AttributesInfo;
using FN::AttributesRef;

class ParticlesState {
 private:
  StringMap<AttributesBlockContainer *> m_container_by_id;
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
  StringMap<AttributesBlockContainer *> &particle_containers();

  /**
   * Get the container corresponding to a particle system name.
   * Asserts when the container does not exist.
   */
  AttributesBlockContainer &particle_container(StringRef name);

  /**
   * Get the name of a container in the context of this particle state.
   */
  StringRefNull particle_container_name(AttributesBlockContainer &container);

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

inline StringMap<AttributesBlockContainer *> &ParticlesState::particle_containers()
{
  return m_container_by_id;
}

inline AttributesBlockContainer &ParticlesState::particle_container(StringRef name)
{
  return *m_container_by_id.lookup(name);
}

inline StringRefNull ParticlesState::particle_container_name(AttributesBlockContainer &container)
{
  StringRefNull result = m_container_by_id.find_key_for_value(&container);
  return result;
}

}  // namespace BParticles
