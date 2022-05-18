/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_dot_export.hh"

#include "FN_lazy_function_graph.hh"

namespace blender::fn {

LazyFunctionGraph::~LazyFunctionGraph()
{
  for (LFNode *node : nodes_) {
    for (LFInputSocket *socket : node->inputs_) {
      std::destroy_at(socket);
    }
    for (LFOutputSocket *socket : node->outputs_) {
      std::destroy_at(socket);
    }
    std::destroy_at(node);
  }
}

LFNode &LazyFunctionGraph::add_node(const LazyFunction &fn)
{
  const Span<LazyFunctionInput> inputs = fn.inputs();
  const Span<LazyFunctionOutput> outputs = fn.outputs();

  LFNode &node = *allocator_.construct<LFNode>().release();
  node.fn_ = &fn;
  node.inputs_ = allocator_.construct_elements_and_pointer_array<LFInputSocket>(inputs.size());
  node.outputs_ = allocator_.construct_elements_and_pointer_array<LFOutputSocket>(outputs.size());

  for (const int i : inputs.index_range()) {
    LFInputSocket &socket = *node.inputs_[i];
    socket.index_in_node_ = i;
    socket.is_input_ = true;
    socket.node_ = &node;
    socket.type_ = inputs[i].type;
  }
  for (const int i : outputs.index_range()) {
    LFOutputSocket &socket = *node.outputs_[i];
    socket.index_in_node_ = i;
    socket.is_input_ = false;
    socket.node_ = &node;
    socket.type_ = outputs[i].type;
  }

  nodes_.append(&node);
  return node;
}

void LazyFunctionGraph::add_link(LFOutputSocket &from, LFInputSocket &to)
{
  BLI_assert(to.origin_ == nullptr);
  to.origin_ = &from;
  from.targets_.append(&to);
}

std::string LFSocket::name() const
{
  const LazyFunction &fn = node_->function();
  if (is_input_) {
    return fn.input_name(index_in_node_);
  }
  return fn.output_name(index_in_node_);
}

std::string LFNode::name() const
{
  return fn_->name();
}

std::string LazyFunctionGraph::to_dot() const
{
  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<const LFNode *, dot::NodeWithSocketsRef> dot_nodes;

  for (const LFNode *node : nodes_) {
    dot::Node &dot_node = digraph.new_node("");
    dot_node.set_background_color("white");

    Vector<std::string> input_names;
    Vector<std::string> output_names;
    for (const LFInputSocket *socket : node->inputs()) {
      input_names.append(socket->name());
    }
    for (const LFOutputSocket *socket : node->outputs()) {
      output_names.append(socket->name());
    }

    dot_nodes.add_new(node,
                      dot::NodeWithSocketsRef(dot_node, node->name(), input_names, output_names));
  }

  for (const LFNode *node : nodes_) {
    for (const LFInputSocket *socket : node->inputs()) {
      const dot::NodeWithSocketsRef &to_dot_node = dot_nodes.lookup(&socket->node());
      const dot::NodePort to_dot_port = to_dot_node.input(socket->index_in_node());

      if (const LFOutputSocket *origin = socket->origin()) {
        dot::NodeWithSocketsRef &from_dot_node = dot_nodes.lookup(&origin->node());
        digraph.new_edge(from_dot_node.output(origin->index_in_node()), to_dot_port);
      }
      else if (const void *default_value = socket->default_value()) {
        const CPPType &type = socket->type();
        const std::string value_string = type.to_string(default_value);
        dot::Node &default_value_dot_node = digraph.new_node(value_string);
        default_value_dot_node.set_shape(dot::Attr_shape::Ellipse);
        digraph.new_edge(default_value_dot_node, to_dot_port);
      }
    }
  }

  return digraph.to_dot_string();
}

}  // namespace blender::fn
