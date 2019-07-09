#pragma once

#include "BLI_math.hpp"
#include "BLI_small_map.hpp"
#include "BLI_string_ref.hpp"

namespace BParticles {

using BLI::float4x4;
using BLI::SmallMap;
using BLI::StringRef;

class WorldState {
 private:
  SmallMap<std::string, float4x4> m_matrices;

 public:
  float4x4 update(StringRef id, float4x4 value)
  {
    std::string id_string = id.to_std_string();
    float4x4 old = m_matrices.lookup_default(id_string, value);
    m_matrices.add_override(id_string, value);
    return old;
  }
};

};  // namespace BParticles
