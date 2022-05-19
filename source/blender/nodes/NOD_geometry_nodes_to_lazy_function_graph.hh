/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_lazy_function_graph.hh"

#include "NOD_node_tree_ref.hh"

struct Object;
struct Depsgraph;

namespace blender::nodes {

using namespace fn::lazy_function_graph_types;

struct GeoNodesLazyFunctionUserData : public fn::LazyFunctionUserData {
  const Object *self_object;
  Depsgraph *depsgraph;
};

struct GeometryNodesLazyFunctionResources {
  Vector<std::unique_ptr<LazyFunction>> functions;
};

void geometry_nodes_to_lazy_function_graph(const NodeTreeRef &tree,
                                           LazyFunctionGraph &graph,
                                           GeometryNodesLazyFunctionResources &resources);

}  // namespace blender::nodes
