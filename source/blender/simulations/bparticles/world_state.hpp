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
  template<typename T> struct OldAndNew {
    T old_value, new_value;
  };

  StringMap<OldAndNew<float3>> m_float3;
  StringMap<OldAndNew<float4x4>> m_float4x4;

 public:
  float3 get_last_and_store_current(StringRef id, float3 current)
  {
    auto *item = m_float3.lookup_ptr(id);
    if (item != nullptr) {
      item->new_value = current;
      return item->old_value;
    }
    else {
      m_float3.add_new(id, {current, current});
      return current;
    }
  }

  float4x4 get_last_and_store_current(StringRef id, float4x4 current)
  {
    auto *item = m_float4x4.lookup_ptr(id);
    if (item != nullptr) {
      item->new_value = current;
      return item->old_value;
    }
    else {
      m_float4x4.add_new(id, {current, current});
      return current;
    }
  }

  void current_step_is_over()
  {
    for (auto &item : m_float3.values()) {
      item.old_value = item.new_value;
    }
    for (auto &item : m_float4x4.values()) {
      item.old_value = item.new_value;
    }
  }
};

};  // namespace BParticles
