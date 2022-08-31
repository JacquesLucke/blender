/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_multi_function.hh"

#include "BKE_node.h"

namespace blender::nodes {

NodeMultiFunctions::NodeMultiFunctions(const bNodeTree &tree)
{
  tree.ensure_topology_cache();
  for (const NodeRef *node : tree.nodes()) {
    bNode *bnode = node->bnode();
    if (bnode->typeinfo->build_multi_function == nullptr) {
      continue;
    }
    NodeMultiFunctionBuilder builder{*bnode, *btree};
    bnode->typeinfo->build_multi_function(builder);
    if (builder.built_fn_ != nullptr) {
      map_.add_new(bnode, {builder.built_fn_, std::move(builder.owned_built_fn_)});
    }
  }
}

}  // namespace blender::nodes
