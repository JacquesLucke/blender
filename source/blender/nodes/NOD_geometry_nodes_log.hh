/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_context_stack_map.hh"
#include "BLI_enumerable_thread_specific.hh"

#include "DNA_node_types.h"

namespace blender::nodes {

class GeoNodesTreeEvalLog {
 public:
  int count = 0;
};

class GeoNodesModifierEvalLog {
 private:
  threading::EnumerableThreadSpecific<ContextStackMap<GeoNodesTreeEvalLog>> logs_;

 public:
  GeoNodesTreeEvalLog &get_local_log(const ContextStack &context_stack)
  {
    return logs_.local().lookup_or_add(context_stack);
  }
};

}  // namespace blender::nodes
