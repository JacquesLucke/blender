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
#include "FN_lazy_function_execute.hh"
#include "FN_lazy_function_graph_executor.hh"
#include "FN_multi_function.hh"

BLI_CPP_TYPE_MAKE(blender::fn::field2::FieldArrayContextValue, CPPTypeFlags::None);

namespace blender::fn::field2 {

namespace lf = lazy_function;

namespace data_flow_graph {

Graph::~Graph()
{
  static_assert(std::is_trivially_destructible_v<FunctionNode>);
  static_assert(std::is_trivially_destructible_v<OutputNode>);
}

FunctionNode &Graph::add_function_node(const OutputSocket &context,
                                       const FieldFunction &fn,
                                       int inputs_num,
                                       int outputs_num,
                                       const void *fn_data)
{
  FunctionNode &node = allocator_.construct_trivial<FunctionNode>();
  node.type_ = NodeType::Function;
  node.context_ = context;
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

  auto port_from_input_socket = [&](const InputSocket &socket) -> dot::NodePort {
    if (socket.node->type() == NodeType::Function) {
      return function_dot_nodes.lookup(static_cast<const FunctionNode *>(socket.node))
          .input(socket.index);
    }
    return *output_dot_nodes.lookup(static_cast<const OutputNode *>(socket.node));
  };
  auto port_from_output_socket = [&](const OutputSocket &socket) -> dot::NodePort {
    if (socket.node->type() == NodeType::Function) {
      return function_dot_nodes.lookup(static_cast<const FunctionNode *>(socket.node))
          .output(socket.index);
    }
    return context_dot_node;
  };

  for (auto item : origins_map_.items()) {
    const InputSocket to = item.key;
    const OutputSocket from = item.value;

    const dot::NodePort from_dot_port = port_from_output_socket(from);
    const dot::NodePort to_dot_port = port_from_input_socket(to);

    digraph.new_edge(from_dot_port, to_dot_port);
  }
  for (const FunctionNode *node : function_nodes_) {
    const OutputSocket &context = node->context();
    const dot::NodePort from_dot_port = port_from_output_socket(context);
    const dot::NodePort to_dot_port = function_dot_nodes.lookup(node).header();

    dot::DirectedEdge &edge = digraph.new_edge(from_dot_port, to_dot_port);
    edge.set_arrowhead(dot::Attr_arrowType::Dot);
    edge.attributes.set("style", "dashed");
    edge.attributes.set("color", "#00000066");
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
}

class MyLazyFunction : public lf::LazyFunction {
 public:
  MyLazyFunction(const int num_field_inputs, const int num_field_outputs)
  {
    debug_name_ = "My Lazy Function";
    inputs_.append({"Context", CPPType::get<FieldArrayContextValue>()});
    for ([[maybe_unused]] const int i : IndexRange(num_field_inputs)) {
      inputs_.append({"Input", CPPType::get<GVArray>()});
    }
    for ([[maybe_unused]] const int i : IndexRange(num_field_outputs)) {
      outputs_.append({"Output", CPPType::get<GVArray>()});
    }
  }

  void execute_impl(lf::Params & /*params*/, const lf::Context & /*context*/) const override
  {
  }
};

class LazyFunctionForConstant : public lf::LazyFunction {
 private:
  const GPointer value_;

 public:
  LazyFunctionForConstant(const GPointer value) : value_(value)
  {
    debug_name_ = "Constant";
    inputs_.append({"Context", CPPType::get<FieldArrayContextValue>()});
    outputs_.append({"Value", CPPType::get<GVArray>()});
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    const FieldArrayContextValue &context = params.get_input<FieldArrayContextValue>(0);
    const int array_size = context.context->array_size();
    params.set_output<GVArray>(0, GVArray::ForSingleRef(*value_.type(), array_size, value_.get()));
  }
};

class LazyFunctionForMultiFunction : public lf::LazyFunction {
 private:
  const MultiFunction &multi_function_;
  Vector<int> input_param_indices_;
  Vector<int> output_param_indices_;

 public:
  LazyFunctionForMultiFunction(const MultiFunction &multi_function)
      : multi_function_(multi_function)
  {
    debug_name_ = "Multi Function";
    inputs_.append({"Context", CPPType::get<FieldArrayContextValue>()});
    for (const int param_index : multi_function.param_indices()) {
      const MFParamType param_type = multi_function.param_type(param_index);
      switch (param_type.category()) {
        case MFParamCategory::SingleInput: {
          inputs_.append({"Input", CPPType::get<GVArray>()});
          input_param_indices_.append(param_index);
          break;
        }
        case MFParamCategory::SingleOutput: {
          outputs_.append({"Output", CPPType::get<GVArray>()});
          output_param_indices_.append(param_index);
          break;
        }
        case MFParamCategory::VectorInput:
        case MFParamCategory::VectorOutput:
        case MFParamCategory::SingleMutable:
        case MFParamCategory::VectorMutable: {
          BLI_assert_unreachable();
          break;
        }
      }
    }
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    const FieldArrayContextValue &context = params.get_input<FieldArrayContextValue>(0);
    const int array_size = context.context->array_size();

    MFParamsBuilder mf_params{multi_function_, array_size};
    MFContextBuilder mf_context;

    for (const int i : input_param_indices_.index_range()) {
      const GVArray &input_varray = params.get_input<GVArray>(i + 1);
      mf_params.add_readonly_single_input(input_varray);
    }
    for (const int i : output_param_indices_.index_range()) {
      if (params.get_output_usage(i) == lf::ValueUsage::Unused) {
        mf_params.add_ignored_single_output();
        continue;
      }
      const MFParamType param_type = multi_function_.param_type(output_param_indices_[i]);
      const CPPType &type = param_type.data_type().single_type();
      void *data = params.get_output_data_ptr(i);
      GArray<> output_array{type, array_size};
      void *buffer = output_array.data();
      type.destruct_n(buffer, array_size);
      new (data) GVArray(GVArray::ForGArray(std::move(output_array)));
      mf_params.add_uninitialized_single_output({type, buffer, array_size});
    }

    multi_function_.call_auto(IndexMask(array_size), mf_params, mf_context);

    for (const int i : outputs_.index_range()) {
      params.output_set(i);
    }
  }
};

void FieldArrayEvaluator::finalize()
{
  BLI_assert(!is_finalized_);
  BLI_SCOPED_DEFER([&]() { is_finalized_ = true; });

  output_nodes_ = build_dfg_for_fields(graph_, fields_);

  Map<dfg::InputSocket, lf::InputSocket *> inputs_map;
  Map<dfg::OutputSocket, lf::OutputSocket *> outputs_map;
  Map<const dfg::FunctionNode *, lf::InputSocket *> context_inputs_map;

  lf::DummyNode &lf_context_node = lf_graph_.add_dummy(
      {}, {&CPPType::get<FieldArrayContextValue>()}, "Context");
  outputs_map.add_new(graph_.context_socket(), &lf_context_node.output(0));

  Vector<lf::InputSocket *> lf_graph_outputs;
  for (const dfg::OutputNode *dfg_output_node : graph_.output_nodes()) {
    lf::DummyNode &lf_output_node = lf_graph_.add_dummy({&CPPType::get<GVArray>()}, {}, "Output");
    inputs_map.add_new({dfg_output_node, 0}, &lf_output_node.input(0));
    lf_graph_outputs.append(&lf_output_node.input(0));
  }

  for (const dfg::FunctionNode *dfg_function_node : graph_.function_nodes()) {
    const FieldFunction &field_fn = dfg_function_node->function();
    const void *field_fn_data = dfg_function_node->fn_data();

    const BackendFlags backends = field_fn.dfg_node_backends(field_fn_data);

    const lf::LazyFunction &lf_fn = [&]() -> const lf::LazyFunction & {
      if (bool(backends & BackendFlags::ConstantValue)) {
        const GPointer value = field_fn.dfg_backend_constant_value(field_fn_data, scope_);
        return scope_.construct<LazyFunctionForConstant>(value);
      }
      else if (bool(backends & BackendFlags::MultiFunction)) {
        const MultiFunction &multi_function = field_fn.dfg_backend_multi_function(field_fn_data,
                                                                                  scope_);
        return scope_.construct<LazyFunctionForMultiFunction>(multi_function);
      }
      else if (bool(backends & BackendFlags::LazyFunction)) {
        return field_fn.dfg_backend_lazy_function(field_fn_data, scope_);
      }
      else {
        return scope_.construct<MyLazyFunction>(dfg_function_node->inputs_num(),
                                                dfg_function_node->outputs_num());
      }
    }();

    lf::FunctionNode &lf_node = lf_graph_.add_function(lf_fn);
    for (const int i : IndexRange(dfg_function_node->inputs_num())) {
      inputs_map.add_new({dfg_function_node, i}, &lf_node.input(i + 1));
    }
    for (const int i : IndexRange(dfg_function_node->outputs_num())) {
      outputs_map.add_new({dfg_function_node, i}, &lf_node.output(i));
    }
    context_inputs_map.add_new(dfg_function_node, &lf_node.input(0));
  }

  for (const dfg::OutputNode *dfg_output_node : graph_.output_nodes()) {
    const dfg::InputSocket dfg_to_socket{dfg_output_node, 0};
    if (std::optional<dfg::OutputSocket> dfg_from_socket = graph_.origin_socket_opt(
            dfg_to_socket)) {
      lf::InputSocket &lf_to_socket = *inputs_map.lookup(dfg_to_socket);
      lf::OutputSocket &lf_from_socket = *outputs_map.lookup(*dfg_from_socket);
      lf_graph_.add_link(lf_from_socket, lf_to_socket);
    }
  }

  for (const dfg::FunctionNode *dfg_function_node : graph_.function_nodes()) {
    for (const int i : IndexRange(dfg_function_node->inputs_num())) {
      const dfg::InputSocket dfg_to_socket{dfg_function_node, i};
      const dfg::OutputSocket dfg_from_socket{graph_.origin_socket(dfg_to_socket)};

      lf::InputSocket &lf_to_socket = *inputs_map.lookup(dfg_to_socket);
      lf::OutputSocket &lf_from_socket = *outputs_map.lookup(dfg_from_socket);
      lf_graph_.add_link(lf_from_socket, lf_to_socket);
    }

    const dfg::OutputSocket dfg_context_origin = dfg_function_node->context();
    lf::InputSocket &lf_context_input = *context_inputs_map.lookup(dfg_function_node);
    lf::OutputSocket &lf_context_origin = *outputs_map.lookup(dfg_context_origin);
    lf_graph_.add_link(lf_context_origin, lf_context_input);
  }

  lf_graph_.update_node_indices();

  lf_graph_executor_.emplace(lf_graph_,
                             Span<const lf::OutputSocket *>{&lf_context_node.output(0)},
                             lf_graph_outputs,
                             nullptr,
                             nullptr);

  std::cout << "\n\n" << lf_graph_.to_dot() << "\n\n";
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
  BLI_assert(mask_.min_array_size() <= context.array_size());
}

void FieldArrayEvaluation::evaluate()
{
  LinearAllocator<> allocator;

  FieldArrayContextValue context_value;
  context_value.context = &context_;

  Array<GMutablePointer> lf_inputs = {&context_value};
  Array<GMutablePointer> lf_outputs(results_.size());
  for (const int i : results_.index_range()) {
    lf_outputs[i] = &results_[i];
    /* TODO: Destruct? */
  }

  Array<std::optional<lf::ValueUsage>> lf_input_usages(1);
  Array<lf::ValueUsage> lf_output_usages(results_.size(), lf::ValueUsage::Used);
  Array<bool> lf_set_outputs(results_.size(), false);

  lf::BasicParams lf_params{*evaluator_.lf_graph_executor_,
                            lf_inputs,
                            lf_outputs,
                            lf_input_usages,
                            lf_output_usages,
                            lf_set_outputs};
  lf::Context lf_context;
  lf_context.storage = evaluator_.lf_graph_executor_->init_storage(allocator);
  evaluator_.lf_graph_executor_->execute(lf_params, lf_context);
  evaluator_.lf_graph_executor_->destruct_storage(lf_context.storage);
}

}  // namespace blender::fn::field2
