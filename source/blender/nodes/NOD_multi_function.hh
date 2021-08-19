/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include "FN_multi_function.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

namespace blender::nodes {

using namespace fn::multi_function_types;

class NodeMultiFunctions;

class NodeMultiFunctionBuilder : NonCopyable, NonMovable {
 private:
  ResourceScope &resource_scope_;
  bNode &node_;
  bNodeTree &tree_;
  const MultiFunction *built_fn_ = nullptr;

  friend NodeMultiFunctions;

 public:
  NodeMultiFunctionBuilder(ResourceScope &resource_scope, bNode &node, bNodeTree &tree);

  void set_matching_fn(const MultiFunction *fn);

  bNode &node();
  bNodeTree &tree();

  ResourceScope &resource_scope();
};

class NodeMultiFunctions {
 private:
  Map<const bNode *, const MultiFunction *> map_;

 public:
  NodeMultiFunctions(const DerivedNodeTree &tree, ResourceScope &resource_scope);

  const MultiFunction *try_get(const DNode &node) const;
};

/* --------------------------------------------------------------------
 * NodeMultiFunctionBuilder inline methods.
 */

inline NodeMultiFunctionBuilder::NodeMultiFunctionBuilder(ResourceScope &resource_scope,
                                                          bNode &node,
                                                          bNodeTree &tree)
    : resource_scope_(resource_scope), node_(node), tree_(tree)
{
}

inline bNode &NodeMultiFunctionBuilder::node()
{
  return node_;
}

inline bNodeTree &NodeMultiFunctionBuilder::tree()
{
  return tree_;
}

inline ResourceScope &NodeMultiFunctionBuilder::resource_scope()
{
  return resource_scope_;
}

/* --------------------------------------------------------------------
 * NodeMultiFunctions inline methods.
 */

inline const MultiFunction *NodeMultiFunctions::try_get(const DNode &node) const
{
  return map_.lookup_default(node->bnode(), nullptr);
}

}  // namespace blender::nodes
