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

LFFunctionNode &LazyFunctionGraph::add_function(const LazyFunction &fn)
{
  const Span<LazyFunctionInput> inputs = fn.inputs();
  const Span<LazyFunctionOutput> outputs = fn.outputs();

  LFFunctionNode &node = *allocator_.construct<LFFunctionNode>().release();
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

LFDummyNode &LazyFunctionGraph::add_dummy(Span<const CPPType *> input_types,
                                          Span<const CPPType *> output_types)
{
  LFDummyNode &node = *allocator_.construct<LFDummyNode>().release();
  node.fn_ = nullptr;
  node.inputs_ = allocator_.construct_elements_and_pointer_array<LFInputSocket>(
      input_types.size());
  node.outputs_ = allocator_.construct_elements_and_pointer_array<LFOutputSocket>(
      output_types.size());

  for (const int i : input_types.index_range()) {
    LFInputSocket &socket = *node.inputs_[i];
    socket.index_in_node_ = i;
    socket.is_input_ = true;
    socket.node_ = &node;
    socket.type_ = input_types[i];
  }
  for (const int i : output_types.index_range()) {
    LFOutputSocket &socket = *node.outputs_[i];
    socket.index_in_node_ = i;
    socket.is_input_ = false;
    socket.node_ = &node;
    socket.type_ = output_types[i];
  }

  nodes_.append(&node);
  return node;
}

void LazyFunctionGraph::add_link(LFOutputSocket &from, LFInputSocket &to)
{
  BLI_assert(to.origin_ == nullptr);
  BLI_assert(from.type_ == to.type_);
  to.origin_ = &from;
  from.targets_.append(&to);
}

void LazyFunctionGraph::remove_link(LFOutputSocket &from, LFInputSocket &to)
{
  BLI_assert(to.origin_ == &from);
  BLI_assert(from.targets_.contains(&to));
  to.origin_ = nullptr;
  from.targets_.remove_first_occurrence_and_reorder(&to);
}

void LazyFunctionGraph::update_node_indices()
{
  for (const int i : nodes_.index_range()) {
    nodes_[i]->index_in_graph_ = i;
  }
}

bool LazyFunctionGraph::node_indices_are_valid() const
{
  for (const int i : nodes_.index_range()) {
    if (nodes_[i]->index_in_graph_ != i) {
      return false;
    }
  }
  return true;
}

std::string LFSocket::name() const
{
  if (node_->is_function()) {
    const LFFunctionNode &fn_node = static_cast<const LFFunctionNode &>(*node_);
    const LazyFunction &fn = fn_node.function();
    if (is_input_) {
      return fn.input_name(index_in_node_);
    }
    return fn.output_name(index_in_node_);
  }
  return "Unnamed";
}

std::string LFNode::name() const
{
  if (fn_ == nullptr) {
    return static_cast<const LFDummyNode *>(this)->name_;
  }
  return fn_->name();
}

std::string LazyFunctionGraph::to_dot() const
{
  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<const LFNode *, dot::NodeWithSocketsRef> dot_nodes;

  for (const LFNode *node : nodes_) {
    dot::Node &dot_node = digraph.new_node("");
    if (node->is_dummy()) {
      dot_node.set_background_color("lightblue");
    }
    else {
      dot_node.set_background_color("white");
    }

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
        std::string value_string;
        if (type.is_printable()) {
          value_string = type.to_string(default_value);
        }
        else {
          value_string = "<" + type.name() + ">";
        }
        dot::Node &default_value_dot_node = digraph.new_node(value_string);
        default_value_dot_node.set_shape(dot::Attr_shape::Ellipse);
        digraph.new_edge(default_value_dot_node, to_dot_port);
      }
    }
  }

  return digraph.to_dot_string();
}

}  // namespace blender::fn
