/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_exec.hh"
#include "NOD_geometry_nodes_to_lazy_function_graph.hh"
#include "NOD_multi_function.hh"
#include "NOD_node_declaration.hh"

#include "BLI_map.hh"

#include "DNA_ID.h"

#include "BKE_geometry_set.hh"
#include "BKE_type_conversions.hh"

#include "FN_field_cpp_type.hh"
#include "FN_lazy_function_graph_executor.hh"

namespace blender::nodes {

using fn::LFParams;
using fn::ValueOrField;
using fn::ValueOrFieldCPPType;
using namespace fn::multi_function_types;

static const CPPType *get_socket_cpp_type(const bNodeSocketType &typeinfo)
{
  const CPPType *type = typeinfo.geometry_nodes_cpp_type;
  if (type == nullptr) {
    return nullptr;
  }
  /* The evaluator only supports types that have special member functions. */
  if (!type->has_special_member_functions()) {
    return nullptr;
  }
  return type;
}

static const CPPType *get_socket_cpp_type(const SocketRef &socket)
{
  return get_socket_cpp_type(*socket.bsocket()->typeinfo);
}

static const CPPType *get_vector_type(const CPPType &type)
{
  if (type.is<GeometrySet>()) {
    return &CPPType::get<Vector<GeometrySet>>();
  }
  if (type.is<ValueOrField<std::string>>()) {
    return &CPPType::get<Vector<ValueOrField<std::string>>>();
  }
  return nullptr;
}

static void lazy_function_interface_from_node(const NodeRef &node,
                                              Vector<const InputSocketRef *> &r_used_inputs,
                                              Vector<const OutputSocketRef *> &r_used_outputs,
                                              Vector<fn::LFInput> &r_inputs,
                                              Vector<fn::LFOutput> &r_outputs)
{
  const bool is_muted = node.is_muted();
  const bool supports_lazyness = node.bnode()->typeinfo->geometry_node_execute_supports_laziness ||
                                 node.bnode()->type == NODE_GROUP;
  const fn::ValueUsage input_usage = supports_lazyness ? fn::ValueUsage::Maybe :
                                                         fn::ValueUsage::Used;
  for (const InputSocketRef *socket : node.inputs()) {
    if (!socket->is_available()) {
      continue;
    }
    const CPPType *type = get_socket_cpp_type(*socket);
    if (type == nullptr) {
      continue;
    }
    if (socket->is_multi_input_socket() && !is_muted) {
      type = get_vector_type(*type);
    }
    /* TODO: Name may not be static. */
    r_inputs.append({socket->identifier().c_str(), *type, input_usage});
    r_used_inputs.append(socket);
  }
  for (const OutputSocketRef *socket : node.outputs()) {
    if (!socket->is_available()) {
      continue;
    }
    const CPPType *type = get_socket_cpp_type(*socket);
    if (type == nullptr) {
      continue;
    }
    r_outputs.append({socket->identifier().c_str(), *type});
    r_used_outputs.append(socket);
  }
}

class GeometryNodeLazyFunction : public LazyFunction {
 private:
  const NodeRef &node_;

 public:
  GeometryNodeLazyFunction(const NodeRef &node,
                           Vector<const InputSocketRef *> &r_used_inputs,
                           Vector<const OutputSocketRef *> &r_used_outputs)
      : node_(node)
  {
    static_name_ = node.name().c_str();
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);
  }

  void execute_impl(LFParams &params) const override
  {
    GeoNodeExecParams geo_params{node_, params};
    const bNode &bnode = *node_.bnode();
    BLI_assert(bnode.typeinfo->geometry_node_execute != nullptr);

    // if (GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(
    //         params.user_data_)) {
    //   static std::mutex m;
    //   std::lock_guard lock{m};
    //   if (user_data->context_stack) {
    //     user_data->context_stack->print_stack(std::cout, bnode.name);
    //   }
    //   else {
    //     std::cout << "No stack: " << bnode.name << "\n";
    //   }
    // }

    bnode.typeinfo->geometry_node_execute(geo_params);
  }
};

class MultiInputLazyFunction : public LazyFunction {
 public:
  MultiInputLazyFunction(const InputSocketRef &socket)
  {
    static_name_ = "Multi Input";
    const CPPType *type = get_socket_cpp_type(socket);
    BLI_assert(type != nullptr);
    BLI_assert(socket.is_multi_input_socket());
    for ([[maybe_unused]] const int i : socket.directly_linked_links().index_range()) {
      inputs_.append({"Input", *type});
    }
    const CPPType *vector_type = get_vector_type(*type);
    BLI_assert(vector_type != nullptr);
    outputs_.append({"Output", *vector_type});
  }

  void execute_impl(LFParams &params) const override
  {
    const CPPType &base_type = *inputs_[0].type;
    base_type.to_static_type_tag<GeometrySet, ValueOrField<std::string>>([&](auto type_tag) {
      using T = typename decltype(type_tag)::type;
      if constexpr (std::is_void_v<T>) {
        /* This type is not support in this node for now. */
        BLI_assert_unreachable();
      }
      else {
        void *output_ptr = params.get_output_data_ptr(0);
        Vector<T> &values = *new (output_ptr) Vector<T>();
        for (const int i : inputs_.index_range()) {
          values.append(params.get_input<T>(i));
        }
        params.output_set(0);
      }
    });
  }
};

class RerouteNodeFunction : public LazyFunction {
 public:
  RerouteNodeFunction(const CPPType &type)
  {
    static_name_ = "Reroute";
    inputs_.append({"Input", type});
    outputs_.append({"Output", type});
  }

  void execute_impl(LFParams &params) const override
  {
    void *input_value = params.try_get_input_data_ptr(0);
    void *output_value = params.get_output_data_ptr(0);
    BLI_assert(input_value != nullptr);
    BLI_assert(output_value != nullptr);
    const CPPType &type = *inputs_[0].type;
    type.move_construct(input_value, output_value);
    params.output_set(0);
  }
};

static void execute_multi_function_on_value_or_field(
    const MultiFunction &fn,
    const std::shared_ptr<MultiFunction> &owned_fn,
    const Span<const ValueOrFieldCPPType *> input_types,
    const Span<const ValueOrFieldCPPType *> output_types,
    const Span<const void *> input_values,
    const Span<void *> output_values)
{
  BLI_assert(fn.param_amount() == input_types.size() + output_types.size());
  BLI_assert(input_types.size() == input_values.size());
  BLI_assert(output_types.size() == output_values.size());

  bool any_input_is_field = false;
  for (const int i : input_types.index_range()) {
    const ValueOrFieldCPPType &type = *input_types[i];
    const void *value_or_field = input_values[i];
    if (type.is_field(value_or_field)) {
      any_input_is_field = true;
      break;
    }
  }

  if (any_input_is_field) {
    Vector<GField> input_fields;
    for (const int i : input_types.index_range()) {
      const ValueOrFieldCPPType &type = *input_types[i];
      const void *value_or_field = input_values[i];
      input_fields.append(type.as_field(value_or_field));
    }

    std::shared_ptr<fn::FieldOperation> operation;
    if (owned_fn) {
      operation = std::make_shared<fn::FieldOperation>(owned_fn, std::move(input_fields));
    }
    else {
      operation = std::make_shared<fn::FieldOperation>(fn, std::move(input_fields));
    }

    for (const int i : output_types.index_range()) {
      const ValueOrFieldCPPType &type = *output_types[i];
      void *value_or_field = output_values[i];
      type.construct_from_field(value_or_field, GField{operation, i});
    }
  }
  else {
    MFParamsBuilder params{fn, 1};
    MFContextBuilder context;

    for (const int i : input_types.index_range()) {
      const ValueOrFieldCPPType &type = *input_types[i];
      const CPPType &base_type = type.base_type();
      const void *value_or_field = input_values[i];
      const void *value = type.get_value_ptr(value_or_field);
      params.add_readonly_single_input(GVArray::ForSingleRef(base_type, 1, value));
    }
    for (const int i : output_types.index_range()) {
      const ValueOrFieldCPPType &type = *output_types[i];
      const CPPType &base_type = type.base_type();
      void *value_or_field = output_values[i];
      type.default_construct(value_or_field);
      void *value = type.get_value_ptr(value_or_field);
      base_type.destruct(value);
      params.add_uninitialized_single_output(GMutableSpan{base_type, value, 1});
    }
    fn.call(IndexRange(1), params, context);
  }
}

class MutedNodeFunction : public LazyFunction {
 private:
  Array<int> input_by_output_index_;

 public:
  MutedNodeFunction(const NodeRef &node,
                    Vector<const InputSocketRef *> &r_used_inputs,
                    Vector<const OutputSocketRef *> &r_used_outputs)
  {
    static_name_ = "Muted";
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);
    for (fn::LFInput &fn_input : inputs_) {
      fn_input.usage = fn::ValueUsage::Maybe;
    }

    for (fn::LFInput &fn_input : inputs_) {
      fn_input.usage = fn::ValueUsage::Unused;
    }

    input_by_output_index_.reinitialize(outputs_.size());
    input_by_output_index_.fill(-1);
    for (const InternalLinkRef *internal_link : node.internal_links()) {
      const int input_i = r_used_inputs.first_index_of_try(&internal_link->from());
      const int output_i = r_used_outputs.first_index_of_try(&internal_link->to());
      if (ELEM(-1, input_i, output_i)) {
        continue;
      }
      input_by_output_index_[output_i] = input_i;
      inputs_[input_i].usage = fn::ValueUsage::Maybe;
    }
  }

  void execute_impl(LFParams &params) const override
  {
    for (const int output_i : outputs_.index_range()) {
      if (params.output_was_set(output_i)) {
        continue;
      }
      const CPPType &output_type = *outputs_[output_i].type;
      void *output_value = params.get_output_data_ptr(output_i);
      const int input_i = input_by_output_index_[output_i];
      if (input_i == -1) {
        output_type.value_initialize(output_value);
        params.output_set(output_i);
        continue;
      }
      const void *input_value = params.try_get_input_data_ptr_or_request(input_i);
      if (input_value == nullptr) {
        continue;
      }
      const CPPType &input_type = *inputs_[input_i].type;
      if (input_type == output_type) {
        input_type.copy_construct(input_value, output_value);
        params.output_set(output_i);
        continue;
      }
      const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
      const auto *from_field_type = dynamic_cast<const ValueOrFieldCPPType *>(&input_type);
      const auto *to_field_type = dynamic_cast<const ValueOrFieldCPPType *>(&output_type);
      if (from_field_type != nullptr && to_field_type != nullptr) {
        const CPPType &from_base_type = from_field_type->base_type();
        const CPPType &to_base_type = to_field_type->base_type();
        if (conversions.is_convertible(from_base_type, to_base_type)) {
          const MultiFunction &multi_fn = *conversions.get_conversion_multi_function(
              MFDataType::ForSingle(from_base_type), MFDataType::ForSingle(to_base_type));
          execute_multi_function_on_value_or_field(
              multi_fn, {}, {from_field_type}, {to_field_type}, {input_value}, {output_value});
        }
        params.output_set(output_i);
        continue;
      }
      output_type.value_initialize(output_value);
      params.output_set(output_i);
    }
  }
};

class MultiFunctionConversion : public LazyFunction {
 private:
  const MultiFunction &fn_;
  const ValueOrFieldCPPType &from_type_;
  const ValueOrFieldCPPType &to_type_;

 public:
  MultiFunctionConversion(const MultiFunction &fn,
                          const ValueOrFieldCPPType &from,
                          const ValueOrFieldCPPType &to)
      : fn_(fn), from_type_(from), to_type_(to)
  {
    static_name_ = "Convert";
    inputs_.append({"From", from});
    outputs_.append({"To", to});
  }

  void execute_impl(LFParams &params) const override
  {
    const void *from_value = params.try_get_input_data_ptr(0);
    void *to_value = params.get_output_data_ptr(0);
    BLI_assert(from_value != nullptr);
    BLI_assert(to_value != nullptr);

    execute_multi_function_on_value_or_field(
        fn_, {}, {&from_type_}, {&to_type_}, {from_value}, {to_value});

    params.output_set(0);
  }
};

class MultiFunctionNode : public LazyFunction {
 private:
  const NodeMultiFunctions::Item fn_item_;
  Vector<const ValueOrFieldCPPType *> input_types_;
  Vector<const ValueOrFieldCPPType *> output_types_;

 public:
  MultiFunctionNode(const NodeRef &node,
                    NodeMultiFunctions::Item fn_item,
                    Vector<const InputSocketRef *> &r_used_inputs,
                    Vector<const OutputSocketRef *> &r_used_outputs)
      : fn_item_(std::move(fn_item))
  {
    BLI_assert(fn_item_.fn != nullptr);
    static_name_ = node.name().c_str();
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);
    for (const fn::LFInput &fn_input : inputs_) {
      input_types_.append(dynamic_cast<const ValueOrFieldCPPType *>(fn_input.type));
    }
    for (const fn::LFOutput &fn_output : outputs_) {
      output_types_.append(dynamic_cast<const ValueOrFieldCPPType *>(fn_output.type));
    }
  }

  void execute_impl(LFParams &params) const override
  {
    Vector<const void *> inputs_values(inputs_.size());
    Vector<void *> outputs_values(outputs_.size());
    for (const int i : inputs_.index_range()) {
      inputs_values[i] = params.try_get_input_data_ptr(i);
    }
    for (const int i : outputs_.index_range()) {
      outputs_values[i] = params.get_output_data_ptr(i);
    }
    execute_multi_function_on_value_or_field(*fn_item_.fn,
                                             fn_item_.owned_fn,
                                             input_types_,
                                             output_types_,
                                             inputs_values,
                                             outputs_values);
    for (const int i : outputs_.index_range()) {
      params.output_set(i);
    }
  }
};

class ComplexInputValueFunction : public LazyFunction {
 private:
  std::function<void(void *)> init_fn_;

 public:
  ComplexInputValueFunction(const CPPType &type, std::function<void(void *)> init_fn)
      : init_fn_(std::move(init_fn))
  {
    static_name_ = "Input";
    outputs_.append({"Output", type});
  }

  void execute_impl(LFParams &params) const override
  {
    void *value = params.get_output_data_ptr(0);
    init_fn_(value);
    params.output_set(0);
  }
};

class GroupNodeFunction : public LazyFunction {
 private:
  const NodeRef &group_node_;
  std::optional<NodeTreeRef> tree_ref_;
  GeometryNodesLazyFunctionResources resources_;
  LazyFunctionGraph graph_;
  std::optional<fn::LazyFunctionGraphExecutor> graph_executor_;

 public:
  GroupNodeFunction(const NodeRef &group_node,
                    Vector<const InputSocketRef *> &r_used_inputs,
                    Vector<const OutputSocketRef *> &r_used_outputs)
      : group_node_(group_node)
  {
    /* Todo: No static name. */
    static_name_ = group_node.name().c_str();
    lazy_function_interface_from_node(
        group_node, r_used_inputs, r_used_outputs, inputs_, outputs_);

    GeometryNodeLazyFunctionMapping mapping;

    bNodeTree *btree = reinterpret_cast<bNodeTree *>(group_node_.bnode()->id);
    BLI_assert(btree != nullptr); /* Todo. */
    tree_ref_.emplace(btree);
    geometry_nodes_to_lazy_function_graph(*tree_ref_, graph_, resources_, mapping);
    graph_.update_node_indices();

    Vector<const LFOutputSocket *> graph_inputs;
    for (const LFOutputSocket *socket : mapping.group_input_sockets) {
      if (socket != nullptr) {
        graph_inputs.append(socket);
      }
    }
    Vector<const LFInputSocket *> graph_outputs;
    for (const NodeRef *node : tree_ref_->nodes_by_type("NodeGroupOutput")) {
      for (const InputSocketRef *socket_ref : node->inputs()) {
        const LFSocket *socket = mapping.dummy_socket_map.lookup_default(socket_ref, nullptr);
        if (socket != nullptr) {
          graph_outputs.append(&socket->as_input());
        }
      }
      break;
    }
    graph_executor_.emplace(graph_, std::move(graph_inputs), std::move(graph_outputs));
  }

  void execute_impl(LFParams &params) const override
  {
    // const ContextStack *parent_context_stack = nullptr;
    // if (GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(
    //         params.user_data_)) {
    //   parent_context_stack = user_data->context_stack;
    // }
    // NodeGroupContextStack context_stack{
    //     parent_context_stack, group_node_.name(), group_node_.bnode()->id->name + 2};
    // if (GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(
    //         params.user_data_)) {
    //   user_data->context_stack = &context_stack;
    // }
    graph_executor_->execute(params);
  }

  void *init_storage(LinearAllocator<> &allocator) const
  {
    return graph_executor_->init_storage(allocator);
  }

  void destruct_storage(void *storage) const
  {
    graph_executor_->destruct_storage(storage);
  }
};

static LFOutputSocket *insert_type_conversion(LazyFunctionGraph &graph,
                                              LFOutputSocket &from_socket,
                                              const CPPType &to_type,
                                              const bke::DataTypeConversions &conversions,
                                              GeometryNodesLazyFunctionResources &resources)
{
  const CPPType &from_type = from_socket.type();
  if (from_type == to_type) {
    return &from_socket;
  }
  const auto *from_field_type = dynamic_cast<const ValueOrFieldCPPType *>(&from_type);
  const auto *to_field_type = dynamic_cast<const ValueOrFieldCPPType *>(&to_type);
  if (from_field_type != nullptr && to_field_type != nullptr) {
    const CPPType &from_base_type = from_field_type->base_type();
    const CPPType &to_base_type = to_field_type->base_type();
    if (conversions.is_convertible(from_base_type, to_base_type)) {
      const MultiFunction &multi_fn = *conversions.get_conversion_multi_function(
          MFDataType::ForSingle(from_base_type), MFDataType::ForSingle(to_base_type));
      auto fn = std::make_unique<MultiFunctionConversion>(
          multi_fn, *from_field_type, *to_field_type);
      LFNode &conversion_node = graph.add_function(*fn);
      resources.functions.append(std::move(fn));
      graph.add_link(from_socket, conversion_node.input(0));
      return &conversion_node.output(0);
    }
  }
  return nullptr;
}

static GMutablePointer get_socket_default_value(LinearAllocator<> &allocator,
                                                const SocketRef &socket_ref)
{
  const bNodeSocketType &typeinfo = *socket_ref.typeinfo();
  const CPPType *type = get_socket_cpp_type(typeinfo);
  if (type == nullptr) {
    return {};
  }
  void *buffer = allocator.allocate(type->size(), type->alignment());
  typeinfo.get_geometry_nodes_cpp_value(*socket_ref.bsocket(), buffer);
  return {type, buffer};
}

static void prepare_socket_default_value(LFInputSocket &socket,
                                         const SocketRef &socket_ref,
                                         GeometryNodesLazyFunctionResources &resources)
{
  GMutablePointer value = get_socket_default_value(resources.allocator, socket_ref);
  if (value.get() == nullptr) {
    return;
  }
  socket.set_default_value(value.get());
  if (!value.type()->is_trivially_destructible()) {
    resources.values_to_destruct.append(value);
  }
}

static void create_init_func_if_necessary(LFInputSocket &socket,
                                          const InputSocketRef &socket_ref,
                                          LazyFunctionGraph &graph,
                                          GeometryNodesLazyFunctionResources &resources)
{
  const NodeRef &node_ref = socket_ref.node();
  const nodes::NodeDeclaration *node_declaration = node_ref.declaration();
  if (node_declaration == nullptr) {
    return;
  }
  const nodes::SocketDeclaration &socket_declaration =
      *node_declaration->inputs()[socket_ref.index()];
  const CPPType &type = socket.type();
  std::function<void(void *)> init_fn;
  if (socket_declaration.input_field_type() == nodes::InputSocketFieldType::Implicit) {
    const bNode &bnode = *node_ref.bnode();
    const bNodeSocketType &socktype = *socket_ref.typeinfo();
    if (socktype.type == SOCK_VECTOR) {
      if (bnode.type == GEO_NODE_SET_CURVE_HANDLES) {
        StringRef side = ((NodeGeometrySetCurveHandlePositions *)bnode.storage)->mode ==
                                 GEO_NODE_CURVE_HANDLE_LEFT ?
                             "handle_left" :
                             "handle_right";
        init_fn = [side](void *r_value) {
          new (r_value) ValueOrField<float3>(bke::AttributeFieldInput::Create<float3>(side));
        };
      }
      else if (bnode.type == GEO_NODE_EXTRUDE_MESH) {
        init_fn = [](void *r_value) {
          new (r_value)
              ValueOrField<float3>(Field<float3>(std::make_shared<bke::NormalFieldInput>()));
        };
      }
      else {
        init_fn = [](void *r_value) {
          new (r_value) ValueOrField<float3>(bke::AttributeFieldInput::Create<float3>("position"));
        };
      }
    }
    else if (socktype.type == SOCK_INT) {
      if (ELEM(bnode.type, FN_NODE_RANDOM_VALUE, GEO_NODE_INSTANCE_ON_POINTS)) {
        init_fn = [](void *r_value) {
          new (r_value)
              ValueOrField<int>(Field<int>(std::make_shared<bke::IDAttributeFieldInput>()));
        };
      }
      else {
        init_fn = [](void *r_value) {
          new (r_value) ValueOrField<int>(Field<int>(std::make_shared<fn::IndexFieldInput>()));
        };
      }
    }
  }
  if (!init_fn) {
    return;
  }
  auto fn = std::make_unique<ComplexInputValueFunction>(type, init_fn);
  LFNode &node = graph.add_function(*fn);
  resources.functions.append(std::move(fn));
  graph.add_link(node.output(0), socket);
}

void geometry_nodes_to_lazy_function_graph(const NodeTreeRef &tree,
                                           LazyFunctionGraph &graph,
                                           GeometryNodesLazyFunctionResources &resources,
                                           GeometryNodeLazyFunctionMapping &mapping)
{
  MultiValueMap<const InputSocketRef *, LFInputSocket *> input_socket_map;
  Map<const OutputSocketRef *, LFOutputSocket *> output_socket_map;
  Map<const InputSocketRef *, LFNode *> multi_input_socket_nodes;

  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();

  resources.node_multi_functions.append(std::make_unique<NodeMultiFunctions>(tree));
  const NodeMultiFunctions &node_multi_functions = *resources.node_multi_functions.last();

  const bNodeTree &btree = *tree.btree();

  Vector<const CPPType *> group_input_types;
  Vector<int> group_input_indices;
  LISTBASE_FOREACH (const bNodeSocket *, socket, &btree.inputs) {
    const CPPType *type = get_socket_cpp_type(*socket->typeinfo);
    if (type != nullptr) {
      const int index = group_input_types.append_and_get_index(type);
      group_input_indices.append(index);
    }
    else {
      group_input_indices.append(-1);
    }
  }
  LFDummyNode &group_input_node = graph.add_dummy({}, group_input_types);
  for (const int i : group_input_indices.index_range()) {
    const int index = group_input_indices[i];
    if (index == -1) {
      mapping.group_input_sockets.append(nullptr);
    }
    else {
      mapping.group_input_sockets.append(&group_input_node.output(index));
    }
  }

  for (const NodeRef *node_ref : tree.nodes()) {
    const bNode &bnode = *node_ref->bnode();
    const bNodeType *node_type = bnode.typeinfo;
    if (node_type == nullptr) {
      continue;
    }
    if (node_ref->is_muted()) {
      Vector<const InputSocketRef *> used_inputs;
      Vector<const OutputSocketRef *> used_outputs;
      auto fn = std::make_unique<MutedNodeFunction>(*node_ref, used_inputs, used_outputs);
      LFNode &node = graph.add_function(*fn);
      resources.functions.append(std::move(fn));
      for (const int i : used_inputs.index_range()) {
        const InputSocketRef &socket_ref = *used_inputs[i];
        input_socket_map.add(&socket_ref, &node.input(i));
        prepare_socket_default_value(node.input(i), socket_ref, resources);
      }
      for (const int i : used_outputs.index_range()) {
        const OutputSocketRef &socket_ref = *used_outputs[i];
        output_socket_map.add_new(&socket_ref, &node.output(i));
      }
      continue;
    }
    switch (node_type->type) {
      case NODE_FRAME: {
        /* Ignored. */
        break;
      }
      case NODE_REROUTE: {
        const CPPType *type = get_socket_cpp_type(node_ref->input(0));
        if (type != nullptr) {
          auto fn = std::make_unique<RerouteNodeFunction>(*type);
          LFNode &node = graph.add_function(*fn);
          resources.functions.append(std::move(fn));
          input_socket_map.add(&node_ref->input(0), &node.input(0));
          output_socket_map.add_new(&node_ref->output(0), &node.output(0));
          prepare_socket_default_value(node.input(0), node_ref->input(0), resources);
        }
        break;
      }
      case NODE_GROUP_INPUT: {
        for (const int i : group_input_indices.index_range()) {
          const int index = group_input_indices[i];
          if (index != -1) {
            const OutputSocketRef &socket_ref = node_ref->output(i);
            LFOutputSocket &socket = group_input_node.output(i);
            output_socket_map.add_new(&socket_ref, &socket);
            mapping.dummy_socket_map.add_new(&socket_ref, &socket);
          }
        }
        break;
      }
      case NODE_GROUP_OUTPUT: {
        Vector<const CPPType *> types;
        Vector<int> indices;
        LISTBASE_FOREACH (const bNodeSocket *, socket, &btree.outputs) {
          const CPPType *type = get_socket_cpp_type(*socket->typeinfo);
          if (type != nullptr) {
            const int index = types.append_and_get_index(type);
            indices.append(index);
          }
          else {
            indices.append(-1);
          }
        }
        LFDummyNode &group_output_node = graph.add_dummy(types, {});
        for (const int i : indices.index_range()) {
          const int index = indices[i];
          if (index != -1) {
            const InputSocketRef &socket_ref = node_ref->input(i);
            LFInputSocket &socket = group_output_node.input(i);
            input_socket_map.add(&socket_ref, &socket);
            mapping.dummy_socket_map.add(&socket_ref, &socket);
            prepare_socket_default_value(socket, socket_ref, resources);
          }
        }
        break;
      }
      case NODE_GROUP: {
        const bool inline_group = false;
        if (inline_group) {
          GeometryNodeLazyFunctionMapping group_mapping;
          bNodeTree *btree = reinterpret_cast<bNodeTree *>(bnode.id);
          resources.sub_tree_refs.append(std::make_unique<NodeTreeRef>(btree));
          const NodeTreeRef &group_ref = *resources.sub_tree_refs.last();
          geometry_nodes_to_lazy_function_graph(group_ref, graph, resources, group_mapping);
          const Span<const NodeRef *> group_output_node_refs = group_ref.nodes_by_type(
              "NodeGroupOutput");
          if (group_output_node_refs.size() == 1) {
            const NodeRef &group_output_node_ref = *group_output_node_refs[0];
            for (const int i : group_output_node_ref.inputs().index_range().drop_back(1)) {
              const InputSocketRef &group_output_ref = group_output_node_ref.input(i);
              const OutputSocketRef &outside_group_output_ref = node_ref->output(i);
              LFInputSocket &group_output_socket =
                  group_mapping.dummy_socket_map.lookup(&group_output_ref)->as_input();
              const CPPType &type = group_output_socket.type();
              LFOutputSocket *group_output_origin = group_output_socket.origin();
              if (group_output_origin == nullptr) {
                auto fn = std::make_unique<RerouteNodeFunction>(type);
                LFNode &node = graph.add_function(*fn);
                resources.functions.append(std::move(fn));
                output_socket_map.add(&outside_group_output_ref, &node.output(0));
                prepare_socket_default_value(node.input(0), group_output_ref, resources);
              }
              else {
                graph.remove_link(*group_output_origin, group_output_socket);
                if (group_output_origin->node().is_dummy()) {
                  const int input_index = group_mapping.group_input_sockets.first_index_of(
                      group_output_origin);
                  auto fn = std::make_unique<RerouteNodeFunction>(type);
                  LFNode &node = graph.add_function(*fn);
                  resources.functions.append(std::move(fn));
                  output_socket_map.add(&outside_group_output_ref, &node.output(0));
                  prepare_socket_default_value(
                      node.input(0), node_ref->input(input_index), resources);
                }
                else {
                  output_socket_map.add(&outside_group_output_ref, group_output_origin);
                }
              }
            }
          }
          else {
            /* TODO */
          }
          for (const int i : group_mapping.group_input_sockets.index_range()) {
            const InputSocketRef &outside_group_input_ref = node_ref->input(i);
            LFOutputSocket &group_input_socket = *group_mapping.group_input_sockets[i];
            const Array<LFInputSocket *> group_input_targets = group_input_socket.targets();
            for (LFInputSocket *group_input_target : group_input_targets) {
              graph.remove_link(group_input_socket, *group_input_target);
              input_socket_map.add(&outside_group_input_ref, group_input_target);
              prepare_socket_default_value(
                  *group_input_target, outside_group_input_ref, resources);
            }
          }
        }
        else {
          Vector<const InputSocketRef *> used_inputs;
          Vector<const OutputSocketRef *> used_outputs;
          auto fn = std::make_unique<GroupNodeFunction>(*node_ref, used_inputs, used_outputs);
          LFNode &node = graph.add_function(*fn);
          resources.functions.append(std::move(fn));
          for (const int i : used_inputs.index_range()) {
            const InputSocketRef &socket_ref = *used_inputs[i];
            BLI_assert(!socket_ref.is_multi_input_socket());
            input_socket_map.add(&socket_ref, &node.input(i));
            prepare_socket_default_value(node.input(i), socket_ref, resources);
          }
          for (const int i : used_outputs.index_range()) {
            const OutputSocketRef &socket_ref = *used_outputs[i];
            output_socket_map.add_new(&socket_ref, &node.output(i));
          }
        }
        break;
      }
      default: {
        if (node_type->geometry_node_execute) {
          Vector<const InputSocketRef *> used_inputs;
          Vector<const OutputSocketRef *> used_outputs;
          auto fn = std::make_unique<GeometryNodeLazyFunction>(
              *node_ref, used_inputs, used_outputs);
          LFNode &node = graph.add_function(*fn);
          resources.functions.append(std::move(fn));

          for (const int i : used_inputs.index_range()) {
            const InputSocketRef &socket_ref = *used_inputs[i];
            LFInputSocket &socket = node.input(i);

            if (socket_ref.is_multi_input_socket()) {
              auto fn = std::make_unique<MultiInputLazyFunction>(socket_ref);
              LFNode &multi_input_node = graph.add_function(*fn);
              resources.functions.append(std::move(fn));
              graph.add_link(multi_input_node.output(0), socket);
              multi_input_socket_nodes.add(&socket_ref, &multi_input_node);
              for (LFInputSocket *multi_input : multi_input_node.inputs()) {
                prepare_socket_default_value(*multi_input, socket_ref, resources);
              }
            }
            else {
              input_socket_map.add(&socket_ref, &socket);
              prepare_socket_default_value(socket, socket_ref, resources);
              const Span<const LinkRef *> links = socket_ref.directly_linked_links();
              if (links.is_empty() || (links.size() == 1 && links[0]->is_muted())) {
                create_init_func_if_necessary(socket, socket_ref, graph, resources);
              }
            }
          }
          for (const int i : used_outputs.index_range()) {
            output_socket_map.add_new(used_outputs[i], &node.output(i));
          }
          break;
        }
        const NodeMultiFunctions::Item &fn_item = node_multi_functions.try_get(*node_ref);
        if (fn_item.fn != nullptr) {
          Vector<const InputSocketRef *> used_inputs;
          Vector<const OutputSocketRef *> used_outputs;
          auto fn = std::make_unique<MultiFunctionNode>(
              *node_ref, fn_item, used_inputs, used_outputs);
          LFNode &node = graph.add_function(*fn);
          resources.functions.append(std::move(fn));

          for (const int i : used_inputs.index_range()) {
            LFInputSocket &socket = node.input(i);
            const InputSocketRef &socket_ref = *used_inputs[i];
            BLI_assert(!socket_ref.is_multi_input_socket());
            input_socket_map.add(&socket_ref, &socket);
            prepare_socket_default_value(socket, socket_ref, resources);
            const Span<const LinkRef *> links = socket_ref.directly_linked_links();
            if (links.is_empty() || (links.size() == 1 && links[0]->is_muted())) {
              create_init_func_if_necessary(socket, socket_ref, graph, resources);
            }
          }
          for (const int i : used_outputs.index_range()) {
            const OutputSocketRef &socket_ref = *used_outputs[i];
            output_socket_map.add(&socket_ref, &node.output(i));
          }
        }
        break;
      }
    }
  }

  for (const auto item : output_socket_map.items()) {
    const OutputSocketRef &from_ref = *item.key;
    LFOutputSocket &from = *item.value;
    const Span<const LinkRef *> links_from_socket = from_ref.directly_linked_links();

    struct TypeWithLinks {
      const CPPType *type;
      Vector<const LinkRef *> links;
    };

    Vector<TypeWithLinks> types_with_links;
    for (const LinkRef *link : links_from_socket) {
      if (link->is_muted()) {
        continue;
      }
      const InputSocketRef &to_socket = link->to();
      if (!to_socket.is_available()) {
        continue;
      }
      const CPPType *to_type = get_socket_cpp_type(to_socket);
      if (to_type == nullptr) {
        continue;
      }
      bool inserted = false;
      for (TypeWithLinks &type_with_links : types_with_links) {
        if (type_with_links.type == to_type) {
          type_with_links.links.append(link);
          inserted = true;
        }
      }
      if (inserted) {
        continue;
      }
      types_with_links.append({to_type, {link}});
    }

    for (const TypeWithLinks &type_with_links : types_with_links) {
      const CPPType &to_type = *type_with_links.type;
      const Span<const LinkRef *> links = type_with_links.links;
      LFOutputSocket *final_from_socket = insert_type_conversion(
          graph, from, to_type, conversions, resources);

      auto make_input_link_or_set_default = [&](LFInputSocket &to_socket) {
        if (final_from_socket != nullptr) {
          graph.add_link(*final_from_socket, to_socket);
        }
        else {
          const void *default_value = to_type.default_value();
          to_socket.set_default_value(default_value);
        }
      };

      for (const LinkRef *link_ref : links) {
        const InputSocketRef &to_socket_ref = link_ref->to();
        if (to_socket_ref.is_multi_input_socket()) {
          /* TODO: Use stored link index, but need to validate it. */
          const int link_index = to_socket_ref.directly_linked_links().first_index_try(link_ref);
          if (to_socket_ref.node().is_muted()) {
            if (link_index == 0) {
              for (LFInputSocket *to : input_socket_map.lookup(&to_socket_ref)) {
                make_input_link_or_set_default(*to);
              }
            }
          }
          else {
            LFNode *multi_input_node = multi_input_socket_nodes.lookup_default(&to_socket_ref,
                                                                               nullptr);
            if (multi_input_node == nullptr) {
              continue;
            }
            make_input_link_or_set_default(multi_input_node->input(link_index));
          }
        }
        else {
          for (LFInputSocket *to : input_socket_map.lookup(&to_socket_ref)) {
            make_input_link_or_set_default(*to);
          }
        }
      }
    }
  }
}

}  // namespace blender::nodes
