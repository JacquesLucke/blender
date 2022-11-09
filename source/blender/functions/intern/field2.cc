/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fn
 */

#include "BLI_dot_export.hh"

#include "FN_field2.hh"

namespace blender::fn::field2 {

namespace data_flow_graph {

Graph::~Graph()
{
  static_assert(std::is_trivially_destructible_v<FunctionNode>);
  static_assert(std::is_trivially_destructible_v<OutputNode>);
}

FunctionNode &Graph::add_function_node(const FieldFunction &fn, const void *fn_data)
{
  FunctionNode &node = allocator_.construct_trivial<FunctionNode>();
  node.type_ = NodeType::Function;
  node.fn_ = &fn;
  node.fn_data_ = fn_data;
  function_nodes_.append(&node);
  return node;
}

OutputNode &Graph::add_output_node(const CPPType &cpp_type)
{
  OutputNode &node = allocator_.construct_trivial<OutputNode>();
  node.type_ = NodeType::Output;
  node.cpp_type_ = &cpp_type;
  output_nodes_.append(&node);
  return node;
}

void Graph::add_link(const OutputSocket &from, const InputSocket &to)
{
  BLI_assert(!this->origin_socket_opt(to).has_value());
  origins_map_.add(to, from);
  targets_map_.add(from, to);
}

std::string Graph::to_dot() const
{
  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<const FunctionNode *, dot::NodeWithSocketsRef> function_dot_nodes;
  Map<const OutputNode *, dot::Node *> output_dot_nodes;

  for (const FunctionNode *node : function_nodes_) {
    dot::Node &dot_node = digraph.new_node("");
    Vector<std::string> input_names;
    Vector<std::string> output_names;
    for (const int index : IndexRange(node->inputs_num())) {
      input_names.append(node->input_name(index));
    }
    for (const int index : IndexRange(node->outputs_num())) {
      output_names.append(node->output_name(index));
    }
    function_dot_nodes.add_new(
        node, dot::NodeWithSocketsRef(dot_node, node->name(), input_names, output_names));
  }
  for (const OutputNode *node : output_nodes_) {
    dot::Node &dot_node = digraph.new_node("");
    dot_node.set_shape(dot::Attr_shape::Diamond);
    output_dot_nodes.add_new(node, &dot_node);
  }

  for (auto item : origins_map_.items()) {
    const InputSocket to = item.key;
    const OutputSocket from = item.value;

    const dot::NodePort from_dot_port = function_dot_nodes.lookup(from.node).output(from.index);
    dot::NodePort to_dot_port = [&]() -> dot::NodePort {
      if (to.node->type() == NodeType::Function) {
        return function_dot_nodes.lookup(static_cast<const FunctionNode *>(to.node))
            .input(to.index);
      }
      return *output_dot_nodes.lookup(static_cast<const OutputNode *>(to.node));
    }();

    digraph.new_edge(from_dot_port, to_dot_port);
  }

  return digraph.to_dot_string();
}

}  // namespace data_flow_graph

}  // namespace blender::fn::field2
