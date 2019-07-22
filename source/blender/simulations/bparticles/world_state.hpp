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

struct InterpolatedFloat {
  float start, end;

  float interpolate(float t)
  {
    return start * (1.0f - t) + end * t;
  }
};

struct InterpolatedFloat3 {
  float3 start, end;

  float3 interpolate(float t)
  {
    return float3::interpolate(start, end, t);
  }
};

struct InterpolatedFloat4x4 {
  /* TODO: store decomposed matrices */
  float4x4 start, end;

  float4x4 interpolate(float t)
  {
    return float4x4::interpolate(start, end, t);
  }
};

class WorldState {
 private:
  template<typename T> struct OldAndNew {
    T old_value, new_value;
  };

  StringMap<OldAndNew<float>> m_float;
  StringMap<OldAndNew<float3>> m_float3;
  StringMap<OldAndNew<float4x4>> m_float4x4;

 public:
  float get_last_and_store_current(StringRef id, float current)
  {
    auto *item = m_float.lookup_ptr(id);
    if (item != nullptr) {
      item->new_value = current;
      return item->old_value;
    }
    else {
      m_float.add_new(id, {current, current});
      return current;
    }
  }

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

  InterpolatedFloat get_interpolated_value(StringRef id, float current)
  {
    float last = this->get_last_and_store_current(id, current);
    return {last, current};
  }

  InterpolatedFloat3 get_interpolated_value(StringRef id, float3 current)
  {
    float3 last = this->get_last_and_store_current(id, current);
    return {last, current};
  }

  InterpolatedFloat4x4 get_interpolated_value(StringRef id, float4x4 current)
  {
    float4x4 last = this->get_last_and_store_current(id, current);
    return {last, current};
  }

  void current_step_is_over()
  {
    for (auto &item : m_float.values()) {
      item.old_value = item.new_value;
    }
    for (auto &item : m_float3.values()) {
      item.old_value = item.new_value;
    }
    for (auto &item : m_float4x4.values()) {
      item.old_value = item.new_value;
    }
  }
};

};  // namespace BParticles
