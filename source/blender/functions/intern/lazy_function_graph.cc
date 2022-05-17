/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_lazy_function_graph.hh"

namespace blender::fn {

LazyFunctionGraph::~LazyFunctionGraph()
{
  for (LFNode *node : nodes_) {
    std::destroy_at(node);
  }
}

}  // namespace blender::fn
