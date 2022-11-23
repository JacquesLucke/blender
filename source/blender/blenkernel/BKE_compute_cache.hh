/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <map>

#include "BLI_compute_context.hh"
#include "BLI_map.hh"

#include "BKE_geometry_set.hh"

namespace blender::bke {

struct GeometryCacheValue {
  int frame;
  float time;
  GeometrySet geometry_set;
};

/* TODO: Clear cache when editing nodes? Only sometimes, when persistent caching is turned off. */
struct SimulationCache {
  Vector<GeometryCacheValue> geometry_per_frame;

  const GeometryCacheValue *value_at_or_before_time(const int frame) const
  {
    const int index = this->index_at_or_before_time(frame);
    if (index >= geometry_per_frame.size()) {
      return nullptr;
    }
    return &geometry_per_frame[index];
  }

  GeometryCacheValue *value_at_or_before_time(const int frame)
  {
    const int index = this->index_at_or_before_time(frame);
    if (index >= geometry_per_frame.size()) {
      return nullptr;
    }
    return &geometry_per_frame[index];
  }

  const GeometryCacheValue *value_before_time(const int frame) const
  {
    const int index = this->index_before_time(frame);
    if (index >= geometry_per_frame.size()) {
      return nullptr;
    }
    return &geometry_per_frame[index];
  }

  GeometryCacheValue *value_at_time(const int frame)
  {
    for (const int i : geometry_per_frame.index_range()) {
      if (geometry_per_frame[i].frame == frame) {
        return &geometry_per_frame[i];
      }
    }
    return nullptr;
  }

  GeometryCacheValue &value_at_time_ensure(const int frame)
  {
    for (const int i : geometry_per_frame.index_range()) {
      if (geometry_per_frame[i].frame == frame) {
        return geometry_per_frame[i];
      }
    }
    const int index = this->index_before_time(frame);
    GeometryCacheValue value{};
    geometry_per_frame.insert(index, value);
    return geometry_per_frame[index];
  }

  void insert(GeometrySet &geometry_set, const int frame, const float time)
  {
    BLI_assert(!this->value_at_time(frame));
    GeometryCacheValue value{};
    value.frame = frame;
    value.time = time;
    value.geometry_set = geometry_set;
    const int index = this->index_before_time(frame);
    geometry_per_frame.insert(index, value);
  }

 private:
  int index_at_or_before_time(const int frame) const
  {
    if (geometry_per_frame.is_empty()) {
      return 0;
    }
    int insert_index = 0;
    for (const int i : geometry_per_frame.index_range()) {
      if (geometry_per_frame[i].frame <= frame) {
        break;
      }
      insert_index++;
    }
    return insert_index;
  }
  int index_before_time(const int frame) const
  {
    if (geometry_per_frame.is_empty()) {
      return 0;
    }
    int insert_index = 0;
    for (const int i : geometry_per_frame.index_range()) {
      if (geometry_per_frame[i].frame < frame) {
        break;
      }
      insert_index++;
    }
    return insert_index;
  }
};

struct ComputeCaches {
 private:
  mutable std::mutex mutex;
  Map<ComputeContextHash, SimulationCache> cache_per_context;

 public:
  ComputeCaches() = default;
  ComputeCaches(const ComputeCaches &other)
  {
    cache_per_context = other.cache_per_context;
  }

  const SimulationCache *lookup_context(const ComputeContextHash &context_hash) const
  {
    std::scoped_lock lock{mutex};
    return cache_per_context.lookup_ptr(context_hash);
  }

  /* TODO: Do we need to use the same context for multiple simulation inputs and outputs in the
   * same node group? If so this won't work at all-- we would need some way to link the two nodes,
   * which might be necessary for the "Run" socket anyway, since it needs to know whether the
   * simulation is running in order to know whether to use the last cache or request a new one. */
  SimulationCache &ensure_for_context(const ComputeContextHash &context_hash)
  {
    std::scoped_lock lock{mutex};
    return cache_per_context.lookup_or_add_default(context_hash);
  }

  bool is_empty() const
  {
    return cache_per_context.is_empty();
  }
};

}  // namespace blender::bke
