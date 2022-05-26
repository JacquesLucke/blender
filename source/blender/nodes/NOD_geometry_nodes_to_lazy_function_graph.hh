/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_lazy_function_graph.hh"

#include "NOD_multi_function.hh"
#include "NOD_node_tree_ref.hh"

struct Object;
struct Depsgraph;

namespace blender::nodes {

using namespace fn::lazy_function_graph_types;

struct GeoNodesLazyFunctionUserData : public fn::LazyFunctionUserData {
  const Object *self_object;
  Depsgraph *depsgraph;
};

struct GeometryNodeLazyFunctionMapping {
  Map<const SocketRef *, LFSocket *> dummy_socket_map;
  Vector<LFOutputSocket *> group_input_sockets;
};

struct GeometryNodesLazyFunctionResources {
  LinearAllocator<> allocator;
  Vector<std::unique_ptr<LazyFunction>> functions;
  Vector<GMutablePointer> values_to_destruct;
  Vector<std::unique_ptr<NodeMultiFunctions>> node_multi_functions;
  Vector<std::unique_ptr<NodeTreeRef>> sub_tree_refs;

  ~GeometryNodesLazyFunctionResources()
  {
    for (GMutablePointer &p : this->values_to_destruct) {
      p.destruct();
    }
  }
};

void geometry_nodes_to_lazy_function_graph(const NodeTreeRef &tree,
                                           LazyFunctionGraph &graph,
                                           GeometryNodesLazyFunctionResources &resources,
                                           GeometryNodeLazyFunctionMapping &mapping);

}  // namespace blender::nodes
