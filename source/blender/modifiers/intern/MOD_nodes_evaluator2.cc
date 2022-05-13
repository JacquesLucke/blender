/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
#include "FN_sgraph_evaluate.hh"
#include "MOD_nodes_evaluator.hh"
#include "NOD_node_tree_ref.hh"

namespace blender::modifiers::geometry_nodes {

using namespace nodes::node_tree_ref_types;

class GeometryNodesExecutor
    : public fn::sgraph::SGraphExecuteSemantics<nodes::NodeTreeRefSGraphAdapter> {
 private:
  const NodeTreeRef &tree_ref_;

 public:
  GeometryNodesExecutor(const NodeTreeRef &tree_ref) : tree_ref_(tree_ref)
  {
  }

  const CPPType *input_socket_type(const NodeID &node, const int input_index) const override
  {
    return get_socket_cpp_type(node->input(input_index));
  }

  const CPPType *output_socket_type(const NodeID &node, const int output_index) const override
  {
    return get_socket_cpp_type(node->output(output_index));
  }

  bool is_multi_input(const NodeID &node, const int input_index) const override
  {
    return node->input(input_index).is_multi_input_socket();
  }

  void load_unlinked_single_input(const NodeID &node,
                                  const int input_index,
                                  GMutablePointer r_value) const override
  {
    get_socket_value(node->input(input_index), r_value.get());
  }

  void foreach_always_required_input_index(const NodeID &node,
                                           const FunctionRef<void(int)> fn) const override
  {
    if (node->typeinfo()->geometry_node_execute_supports_laziness) {
      return;
    }
    for (const InputSocketRef *socket : node->inputs()) {
      if (socket->is_available() && get_socket_cpp_type(*socket) != nullptr) {
        fn(socket->index());
      }
    }
  }

  void execute_node(const NodeID &node, fn::sgraph::ExecuteNodeParams &params) const override
  {
    UNUSED_VARS(node, params);
  }
};

}  // namespace blender::modifiers::geometry_nodes


*/
