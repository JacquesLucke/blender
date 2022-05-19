/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_to_lazy_function_graph.hh"

namespace blender::modifiers::geometry_nodes {
using nodes::SocketRef;
const CPPType *get_socket_cpp_type(const SocketRef &socket);
}  // namespace blender::modifiers::geometry_nodes

namespace blender::nodes {

class GeometryNodeLazyFunction : public LazyFunction {
 public:
  GeometryNodeLazyFunction(const NodeRef &node)
  {
    for (const InputSocketRef *socket : node.inputs()) {
      if (!socket->is_available()) {
        continue;
      }
      const CPPType *type = modifiers::geometry_nodes::get_socket_cpp_type(*socket);
      if (type == nullptr) {
        continue;
      }
      /* TODO: Name may not be static. */
      inputs_.append({socket->identifier().c_str(), *type});
    }
    for (const OutputSocketRef *socket : node.outputs()) {
      if (!socket->is_available()) {
        continue;
      }
      const CPPType *type = modifiers::geometry_nodes::get_socket_cpp_type(*socket);
      if (type == nullptr) {
        continue;
      }
      outputs_.append({socket->identifier().c_str(), *type});
    }
  }

  void execute_impl(fn::LazyFunctionParams &params) const override
  {
    UNUSED_VARS(params);
  }
};

void geometry_nodes_to_lazy_function_graph(const NodeTreeRef &tree,
                                           LazyFunctionGraph &graph,
                                           GeometryNodesLazyFunctionResources &resources)
{
  for (const NodeRef *node_ref : tree.nodes()) {
    auto fn = std::make_unique<GeometryNodeLazyFunction>(*node_ref);
    LFNode &node = graph.add_node(*fn);
    resources.functions.append(std::move(fn));
  }
}

}  // namespace blender::nodes
