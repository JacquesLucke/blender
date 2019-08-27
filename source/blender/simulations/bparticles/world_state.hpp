#pragma once

#include "BLI_math.hpp"
#include "BLI_map.hpp"
#include "BLI_string_ref.hpp"
#include "BLI_string_map.hpp"

namespace BParticles {

using BLI::float3;
using BLI::float4x4;
using BLI::Map;
using BLI::StringMap;
using BLI::StringRef;

struct VaryingFloat {
  float start, end;

  float interpolate(float t)
  {
    return start * (1.0f - t) + end * t;
  }
};

struct VaryingFloat3 {
  float3 start, end;

  float3 interpolate(float t)
  {
    return float3::interpolate(start, end, t);
  }
};

struct VaryingFloat4x4 {
  /* TODO: store decomposed matrices */
  float4x4 start, end;

  float4x4 interpolate(float t)
  {
    return float4x4::interpolate(start, end, t);
  }
};

class WorldTransition;

class WorldState {
 private:
  StringMap<float> m_states_float;
  StringMap<float3> m_states_float3;

  friend WorldTransition;

 public:
  void store_state(StringRef main_id, StringRef sub_id, float value)
  {
    m_states_float.add_new(main_id + sub_id, value);
  }

  void store_state(StringRef main_id, StringRef sub_id, float3 value)
  {
    m_states_float3.add_new(main_id + sub_id, value);
  }
};

class WorldTransition {
 private:
  WorldState &m_old_state;
  WorldState &m_new_state;

 public:
  WorldTransition(WorldState &old_state, WorldState &new_state)
      : m_old_state(old_state), m_new_state(new_state)
  {
  }

  VaryingFloat update_float(StringRef main_id, StringRef sub_id, float current)
  {
    std::string id = main_id + sub_id;
    m_new_state.store_state(main_id, sub_id, current);
    float old_value = m_old_state.m_states_float.lookup_default(id, current);
    return {old_value, current};
  }

  VaryingFloat3 update_float3(StringRef main_id, StringRef sub_id, float3 current)
  {
    std::string id = main_id + sub_id;
    m_new_state.store_state(main_id, sub_id, current);
    float3 old_value = m_old_state.m_states_float3.lookup_default(id, current);
    return {old_value, current};
  }
};

}  // namespace BParticles
