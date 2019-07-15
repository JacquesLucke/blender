#pragma once

#include "BLI_math.hpp"
#include "BLI_small_map.hpp"
#include "BLI_string_ref.hpp"
#include "BLI_string_map.hpp"

namespace BParticles {

using BLI::float3;
using BLI::float4x4;
using BLI::SmallMap;
using BLI::StringMap;
using BLI::StringRef;

class WorldState {
 private:
  StringMap<float4x4> m_matrices;
  StringMap<float3> m_last_positions;
  StringMap<float3> m_current_positions;

 public:
  float3 get_last_and_store_current(StringRef id, float3 current)
  {
    m_current_positions.add_override(id, current);
    return m_last_positions.lookup_default(id, current);
  }

  void current_step_is_over()
  {
    m_last_positions = StringMap<float3>(m_current_positions);
  }

  float4x4 update(StringRef id, float4x4 value)
  {
    std::string id_string = id.to_std_string();
    float4x4 old = m_matrices.lookup_default(id_string, value);
    m_matrices.add_override(id_string, value);
    return old;
  }
};

};  // namespace BParticles
