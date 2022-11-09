/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fn
 */

#include "BLI_cpp_type_make.hh"
#include "BLI_dot_export.hh"
#include "BLI_stack.hh"

#include "FN_field2.hh"

BLI_CPP_TYPE_MAKE(FieldArrayContextValue,
                  blender::fn::field2::FieldArrayContextValue,
                  CPPTypeFlags::None);

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
    dot::Node &dot_node = digraph.new_node("Output");
    dot_node.set_shape(dot::Attr_shape::Diamond);
    output_dot_nodes.add_new(node, &dot_node);
  }

  dot::Node &context_dot_node = digraph.new_node("Context");
  context_dot_node.set_shape(dot::Attr_shape::Ellipse);

  for (auto item : origins_map_.items()) {
    const InputSocket to = item.key;
    const OutputSocket from = item.value;

    const dot::NodePort from_dot_port = [&]() -> dot::NodePort {
      if (from.node->type() == NodeType::Function) {
        return function_dot_nodes.lookup(static_cast<const FunctionNode *>(from.node))
            .output(from.index);
      }
      return context_dot_node;
    }();
    const dot::NodePort to_dot_port = [&]() -> dot::NodePort {
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

struct FieldSocketKey {
  GFieldRef field;
  dfg::OutputSocket context;

  uint64_t hash() const
  {
    return get_default_hash_2(field, context);
  }

  friend bool operator==(const FieldSocketKey &a, const FieldSocketKey &b)
  {
    return a.field == b.field && a.context == b.context;
  }
};

Vector<dfg::OutputNode *> build_dfg_for_fields(dfg::Graph &graph, Span<GFieldRef> fields)
{
  Map<FieldSocketKey, dfg::OutputSocket> built_sockets_map;
  Map<dfg::InputSocket, FieldSocketKey> origins_map;
  Stack<FieldSocketKey> sockets_to_build;

  const dfg::OutputSocket main_context_socket = graph.context_socket();

  Vector<dfg::OutputNode *> output_nodes;
  for (const GFieldRef &field : fields) {
    dfg::OutputNode &output_node = graph.add_output_node(field.cpp_type());
    const dfg::InputSocket output_node_socket = {&output_node, 0};
    const FieldSocketKey key = {field, main_context_socket};
    origins_map.add_new(output_node_socket, key);
    sockets_to_build.push(key);
  }

  while (!sockets_to_build.is_empty()) {
    const FieldSocketKey key = sockets_to_build.pop();
    if (built_sockets_map.contains(key)) {
      continue;
    }

    const FieldNode &field_node = key.field.node();
    const FieldFunction &field_function = field_node.function();
    DfgFunctionBuilder builder{graph, key.context, field_function};
    field_function.dfg_build(builder);

    const Span<DfgFunctionBuilder::InputInfo> built_inputs = builder.built_inputs();
    const Span<DfgFunctionBuilder::OutputInfo> built_outputs = builder.built_outputs();

    const Span<GField> field_node_inputs = field_node.inputs();
    for (const int i : IndexRange(field_function.inputs_num())) {
      FieldSocketKey origin_key = {field_node_inputs[i], built_inputs[i].context};
      origins_map.add_new(built_inputs[i].socket, origin_key);
      sockets_to_build.push(origin_key);
    }
    for (const int i : IndexRange(field_function.outputs_num())) {
      FieldSocketKey output_key = {GFieldRef{field_node, i}, key.context};
      built_sockets_map.add_new(output_key, built_outputs[i].socket);
    }
  }

  for (auto item : origins_map.items()) {
    const dfg::InputSocket &to = item.key;
    const dfg::OutputSocket &from = built_sockets_map.lookup(item.value);

    graph.add_link(from, to);
  }

  return output_nodes;
}

void FieldArrayEvaluator::finalize()
{
  BLI_assert(is_finalized_);
  BLI_SCOPED_DEFER([&]() { is_finalized_ = true; });

  output_nodes_ = build_dfg_for_fields(graph_, fields_);
}

FieldArrayEvaluation::FieldArrayEvaluation(const FieldArrayEvaluator &evaluator,
                                           const FieldArrayContext &context)
    : evaluator_(evaluator), context_(context)
{
  BLI_assert(evaluator_.is_finalized_);
}

}  // namespace blender::fn::field2
