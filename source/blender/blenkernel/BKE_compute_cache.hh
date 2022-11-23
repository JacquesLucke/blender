/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_compute_context.hh"
#include "BLI_map.hh"

#include "BKE_geometry_set.hh"

namespace blender::bke {

struct CacheData {
  Map<int, GeometrySet> geometry_per_frame;
};

struct ComputeCaches {
  Map<ComputeContextHash, CacheData> cache_per_context;
};

}  // namespace blender::bke
