#pragma once

#include "BKE_attributes_block_container.hpp"

namespace BParticles {

using BKE::attribute_type_by_type;
using BKE::AttributesBlock;
using BKE::AttributesBlockContainer;
using BKE::AttributesInfo;
using BKE::AttributesRef;
using BKE::AttributeType;
using BLI::ArrayRef;
using BLI::IndexRange;
using BLI::Map;
using BLI::MutableArrayRef;
using BLI::Set;
using BLI::SetVector;
using BLI::StringMap;
using BLI::StringRef;
using BLI::StringRefNull;
using BLI::Vector;

class ParticlesState {
 private:
  StringMap<AttributesBlockContainer *> m_container_by_id;

 public:
  ParticlesState() = default;
  ParticlesState(ParticlesState &other) = delete;
  ~ParticlesState();

  /**
   * Access the mapping from particle type names to their corresponding containers.
   */
  StringMap<AttributesBlockContainer *> &particle_containers();

  /**
   * Get the container corresponding to a particle type name.
   * Asserts when the container does not exist.
   */
  AttributesBlockContainer &particle_container(StringRef name);

  /**
   * Get the name of a container in the context of this particle state.
   */
  StringRefNull particle_container_name(AttributesBlockContainer &container);
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
