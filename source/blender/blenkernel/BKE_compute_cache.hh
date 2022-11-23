/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <map>

#include "BLI_compute_context.hh"
#include "BLI_map.hh"

#include "BKE_geometry_set.hh"

namespace blender::bke {

struct GeometryCacheValue {
  int frame;
  GeometrySet geometry_set;
};

struct CacheData {
  Vector<GeometryCacheValue> geometry_per_frame;

  GeometrySet *first_item_before(const int frame)
  {
    if (geometry_per_frame.is_empty()) {
      return nullptr;
    }
    if (frame < geometry_per_frame.first().frame) {
      return nullptr;
    }

    GeometryCacheValue *last_value = nullptr;
    for (int i = geometry_per_frame.size() - 1; i > 0; i--) {
      if (geometry_per_frame[i].frame > frame) {
        break;
      }
      last_value = &geometry_per_frame[i];
    }

    return last_value ? &last_value->geometry_set : nullptr;
  }
};

struct ComputeCaches {
  Map<ComputeContextHash, CacheData> cache_per_context;
};

}  // namespace blender::bke
