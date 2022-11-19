/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fn
 */

#include "BLI_cpp_type_make.hh"
#include "BLI_dot_export.hh"
#include "BLI_noise.hh"
#include "BLI_rand.hh"
#include "BLI_stack.hh"

#include "FN_field2.hh"
#include "FN_multi_function.hh"

BLI_CPP_TYPE_MAKE(blender::fn::field2::FieldArrayContextValue, CPPTypeFlags::None);

namespace blender::fn::field2 {

namespace data_flow_graph {

Graph::~Graph()
{
  static_assert(std::is_trivially_destructible_v<FunctionNode>);
  static_assert(std::is_trivially_destructible_v<OutputNode>);
}

FunctionNode &Graph::add_function_node(const FieldFunction &fn,
                                       int inputs_num,
                                       int outputs_num,
                                       const void *fn_data)
{
  FunctionNode &node = allocator_.construct_trivial<FunctionNode>();
  node.type_ = NodeType::Function;
  node.inputs_num_ = inputs_num;
  node.outputs_num_ = outputs_num;
  node.fn_ = &fn;
  node.fn_data_ = fn_data;
  function_nodes_.append(&node);
  return node;
}

OutputNode &Graph::add_output_node(const CPPType &cpp_type)
{
  OutputNode &node = allocator_.construct_trivial<OutputNode>();
  node.type_ = NodeType::Output;
  node.inputs_num_ = 1;
  node.outputs_num_ = 0;
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

std::string Graph::to_dot(const ToDotSettings &settings) const
{
  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<const FunctionNode *, dot::NodeWithSocketsRef> function_dot_nodes;
  Map<const OutputNode *, dot::Node *> output_dot_nodes;

  auto cluster_id_to_color = [](const uint32_t id) {
    const float hue = noise::hash_to_float(id);
    std::stringstream ss;
    ss << hue << " 0.5 1.0";
    return ss.str();
  };

  for (const FunctionNode *node : function_nodes_) {
    dot::Node &dot_node = digraph.new_node("");

    if (settings.cluster_ids_map.contains(node)) {
      const uint32_t id = settings.cluster_ids_map.lookup(node);
      dot_node.set_background_color(cluster_id_to_color(id));
    }

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

    if (settings.cluster_ids_map.contains(node)) {
      const uint32_t id = settings.cluster_ids_map.lookup(node);
      dot_node.set_background_color(cluster_id_to_color(id));
    }

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
    output_nodes.append(&output_node);
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

FieldArrayEvaluator::~FieldArrayEvaluator()
{
  for (GMutablePointer value : constant_outputs_) {
    value.destruct();
  }
}

void FieldArrayEvaluator::finalize()
{
  BLI_assert(!is_finalized_);
  BLI_SCOPED_DEFER([&]() { is_finalized_ = true; });

  output_nodes_ = build_dfg_for_fields(graph_, fields_);
  this->find_context_dependent_nodes();

  for (const int i : output_nodes_.index_range()) {
    const dfg::OutputNode *node = output_nodes_[i];
    if (context_dependent_nodes_.contains(node)) {
      varying_output_indices_.append(i);
    }
    else {
      constant_output_indices_.append(i);
    }
  }

  this->evaluate_constant_outputs();

  /* TODO: Use topology sort to turn remove the quadratic complexity. */
  RandomNumberGenerator rng{23};
  Map<const dfg::Node *, uint32_t> cluster_ids_map;
  for (const dfg::FunctionNode *node : graph_.function_nodes()) {
    const BackendFlags backends = node->function().dfg_node_backends(node->fn_data());
    if (!bool(backends & BackendFlags::LazyFunction)) {
      continue;
    }
    {
      /* Propagate cluster id forwards. */
      Set<const dfg::Node *> pushed_nodes;
      Stack<const dfg::Node *> nodes_to_check;
      const uint32_t forward_id = rng.get_uint32();
      nodes_to_check.push(node);
      pushed_nodes.add_new(node);
      while (!nodes_to_check.is_empty()) {
        const dfg::Node *node = nodes_to_check.pop();
        cluster_ids_map.lookup_or_add(node, 0) ^= forward_id;
        for (const int output_index : IndexRange(node->outputs_num())) {
          for (const dfg::InputSocket target_socket :
               graph_.target_sockets({node, output_index})) {
            const dfg::Node *target_node = target_socket.node;
            if (pushed_nodes.add(target_node)) {
              nodes_to_check.push(target_node);
            }
          }
        }
      }
    }
    {
      /* Propagate cluster id backwards. */
      Set<const dfg::Node *> pushed_nodes;
      Stack<const dfg::Node *> nodes_to_check;
      const uint32_t backward_id = rng.get_uint32();
      nodes_to_check.push(node);
      pushed_nodes.add_new(node);
      while (!nodes_to_check.is_empty()) {
        const dfg::Node *node = nodes_to_check.pop();
        cluster_ids_map.lookup_or_add(node, 0) ^= backward_id;
        for (const int input_index : IndexRange(node->inputs_num())) {
          const dfg::OutputSocket origin_socket = graph_.origin_socket({node, input_index});
          const dfg::Node *origin_node = origin_socket.node;
          if (pushed_nodes.add(origin_node)) {
            nodes_to_check.push(origin_node);
          }
        }
      }
    }
  }
}

void FieldArrayEvaluator::find_context_dependent_nodes()
{
  const dfg::ContextNode &main_context_node = graph_.context_node();
  Stack<const dfg::Node *> nodes_to_check;
  nodes_to_check.push(&main_context_node);
  context_dependent_nodes_.add_new(&main_context_node);

  while (!nodes_to_check.is_empty()) {
    const dfg::Node *node = nodes_to_check.pop();
    for (const int i : IndexRange(node->outputs_num())) {
      const dfg::OutputSocket output_socket{node, i};
      for (const dfg::InputSocket &target : graph_.target_sockets(output_socket)) {
        const dfg::Node *target_node = target.node;
        if (context_dependent_nodes_.add(target_node)) {
          nodes_to_check.push(target_node);
        }
      }
    }
  }
}

void FieldArrayEvaluator::evaluate_constant_outputs()
{
  LinearAllocator<> &allocator = scope_.linear_allocator();
  for (const int output_index : constant_output_indices_) {
    const dfg::OutputNode *node = output_nodes_[output_index];
    const CPPType &type = node->cpp_type();
    void *buffer = allocator.allocate(type.size(), type.alignment());
    GMutablePointer value{type, buffer};
    this->evaluate_constant_input_socket({node, 0}, value);
    constant_outputs_.append(value);
  }
}

void FieldArrayEvaluator::evaluate_constant_input_socket(const dfg::InputSocket &socket_to_compute,
                                                         GMutablePointer r_value)
{
  const dfg::OutputSocket &output_socket = graph_.origin_socket(socket_to_compute);
  BLI_assert(output_socket.node->type() == dfg::NodeType::Function);
  const dfg::FunctionNode &node = *static_cast<const dfg::FunctionNode *>(output_socket.node);
  const FieldFunction &field_function = node.function();
  const void *fn_data = node.fn_data();
  const BackendFlags backends = field_function.dfg_node_backends(fn_data);
  const CPPType &type_to_compute = *r_value.type();

  if (bool(backends & BackendFlags::ConstantValue)) {
    const GPointer value = field_function.dfg_backend_constant_value(fn_data, scope_);
    BLI_assert(*value.type() == type_to_compute);
    type_to_compute.copy_construct(value.get(), r_value.get());
    return;
  }
  if (bool(backends & BackendFlags::MultiFunction)) {
    const MultiFunction &fn = field_function.dfg_backend_multi_function(fn_data, scope_);
    MFParamsBuilder params{fn, 1};
    for (const int input_index : IndexRange(node.inputs_num())) {
      const int param_index = input_index;
      const MFParamType param_type = fn.param_type(param_index);
      BLI_assert(param_type.category() == MFParamCategory::SingleInput);

      const CPPType &input_type = param_type.data_type().single_type();
      BUFFER_FOR_CPP_TYPE_VALUE(input_type, buffer);
      GMutablePointer input_value{input_type, buffer};

      this->evaluate_constant_input_socket({&node, input_index}, input_value);
      params.add_readonly_single_input(GVArray::ForSingle(input_type, 1, buffer));
      input_value.destruct();
    }
    for (const int output_index : IndexRange(node.outputs_num())) {
      const int param_index = output_index + node.inputs_num();
      const MFParamType param_type = fn.param_type(param_index);
      BLI_assert(param_type.category() == MFParamCategory::SingleOutput);

      const CPPType &output_type = param_type.data_type().single_type();

      if (output_index == socket_to_compute.index) {
        BLI_assert(output_type == type_to_compute);
        params.add_uninitialized_single_output(GMutableSpan{output_type, r_value.get(), 1});
      }
      else {
        params.add_ignored_single_output();
      }
    }
    MFContextBuilder context;
    fn.call(IndexRange(1), params, context);
    return;
  }

  /* Use default value if no backend works. */
  type_to_compute.copy_construct(type_to_compute.default_value(), r_value.get());
}

FieldArrayEvaluation::FieldArrayEvaluation(const FieldArrayEvaluator &evaluator,
                                           const FieldArrayContext &context,
                                           const IndexMask *mask)
    : evaluator_(evaluator),
      context_(context),
      mask_(*mask),
      results_(evaluator_.output_nodes_.size())
{
  BLI_assert(evaluator_.is_finalized_);
}

void FieldArrayEvaluation::evaluate()
{
  for (const int i : evaluator_.constant_output_indices_.index_range()) {
    const int output_index = evaluator_.constant_output_indices_[i];
    const GPointer value = evaluator_.constant_outputs_[i];
    results_[output_index] = GVArray::ForSingleRef(
        *value.type(), mask_.min_array_size(), value.get());
  }
}

}  // namespace blender::fn::field2
