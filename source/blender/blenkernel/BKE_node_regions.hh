/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_array.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

namespace blender::bke {

struct NTreeRegionBounds {
  Vector<const bNode *> inputs;
  Vector<const bNode *> outputs;
};

struct NTreeRegion {
  std::optional<int> parent_region;
  Vector<int> children_regions;
  Vector<const bNode *> contained_nodes;
  bool is_in_cycle = false;
};

struct NTreeRegionResult {
  Array<NTreeRegion> regions;
  bool is_valid = false;
};

NTreeRegionResult analyze_node_context_regions(const bNodeTree &ntree,
                                               Span<NTreeRegionBounds> region_bounds);

}  // namespace blender::bke
