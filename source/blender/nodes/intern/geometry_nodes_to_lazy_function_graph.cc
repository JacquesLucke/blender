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

static const CPPType *get_socket_cpp_type(const bNodeSocket &socket)
{
  return get_socket_cpp_type(*socket.typeinfo);
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

static void lazy_function_interface_from_node(const bNode &node,
                                              Vector<const bNodeSocket *> &r_used_inputs,
                                              Vector<const bNodeSocket *> &r_used_outputs,
                                              Vector<lf::Input> &r_inputs,
                                              Vector<lf::Output> &r_outputs)
{
  const bool is_muted = node.is_muted();
  const bool supports_lazyness = node.typeinfo->geometry_node_execute_supports_laziness ||
                                 node.is_group();
  const lf::ValueUsage input_usage = supports_lazyness ? lf::ValueUsage::Maybe :
                                                         lf::ValueUsage::Used;
  for (const bNodeSocket *socket : node.input_sockets()) {
    if (!socket->is_available()) {
      continue;
    }
    const CPPType *type = get_socket_cpp_type(*socket);
    if (type == nullptr) {
      continue;
    }
    if (socket->is_multi_input() && !is_muted) {
      type = get_vector_type(*type);
    }
    /* TODO: Name may not be static. */
    r_inputs.append({socket->identifier, *type, input_usage});
    r_used_inputs.append(socket);
  }
  for (const bNodeSocket *socket : node.output_sockets()) {
    if (!socket->is_available()) {
      continue;
    }
    const CPPType *type = get_socket_cpp_type(*socket);
    if (type == nullptr) {
      continue;
    }
    r_outputs.append({socket->identifier, *type});
    r_used_outputs.append(socket);
  }
}

/**
 * Used for most normal geometry nodes like Subdivision Surface and Set Position.
 */
class LazyFunctionForGeometryNode : public LazyFunction {
 private:
  const bNode &node_;

 public:
  LazyFunctionForGeometryNode(const bNode &node,
                              Vector<const bNodeSocket *> &r_used_inputs,
                              Vector<const bNodeSocket *> &r_used_outputs)
      : node_(node)
  {
    static_name_ = node.name;
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    GeoNodeExecParams geo_params{node_, params, context};
    BLI_assert(node_.typeinfo->geometry_node_execute != nullptr);

    GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
    BLI_assert(user_data != nullptr);

    geo_eval_log::TimePoint start_time = geo_eval_log::Clock::now();
    node_.typeinfo->geometry_node_execute(geo_params);
    geo_eval_log::TimePoint end_time = geo_eval_log::Clock::now();

    geo_eval_log::GeoTreeLogger *tree_logger =
        &user_data->modifier_data->eval_log->get_local_tree_logger(*user_data->context_stack);
    if (tree_logger != nullptr) {
      tree_logger->node_execution_times.append_as(node_.name, start_time, end_time);
    }
  }
};

/**
 * Used to gather all inputs of a multi-input socket.
 */
class LazyFunctionForMultiInput : public LazyFunction {
 private:
  const CPPType *base_type_;

 public:
  LazyFunctionForMultiInput(const bNodeSocket &socket)
  {
    static_name_ = "Multi Input";
    base_type_ = get_socket_cpp_type(socket);
    BLI_assert(base_type_ != nullptr);
    BLI_assert(socket.is_multi_input());
    for (const bNodeLink *link : socket.directly_linked_links()) {
      if (!link->is_muted()) {
        inputs_.append({"Input", *base_type_});
      }
    }
    const CPPType *vector_type = get_vector_type(*base_type_);
    BLI_assert(vector_type != nullptr);
    outputs_.append({"Output", *vector_type});
  }

  void execute_impl(lf::Params &params, const lf::Context &UNUSED(context)) const override
  {
    base_type_->to_static_type_tag<GeometrySet, ValueOrField<std::string>>([&](auto type_tag) {
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

/**
 * Simple lazy-function that just forwards the input.
 */
class LazyFunctionForRerouteNode : public LazyFunction {
 public:
  LazyFunctionForRerouteNode(const CPPType &type)
  {
    static_name_ = "Reroute";
    inputs_.append({"Input", type});
    outputs_.append({"Output", type});
  }

  void execute_impl(lf::Params &params, const lf::Context &UNUSED(context)) const override
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

class LazyFunctionForMutedNode : public LazyFunction {
 private:
  Array<int> input_by_output_index_;

 public:
  LazyFunctionForMutedNode(const bNode &node,
                           Vector<const bNodeSocket *> &r_used_inputs,
                           Vector<const bNodeSocket *> &r_used_outputs)
  {
    static_name_ = "Muted";
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);
    for (lf::Input &fn_input : inputs_) {
      fn_input.usage = lf::ValueUsage::Maybe;
    }

    for (lf::Input &fn_input : inputs_) {
      fn_input.usage = lf::ValueUsage::Unused;
    }

    input_by_output_index_.reinitialize(outputs_.size());
    input_by_output_index_.fill(-1);
    for (const bNodeLink *internal_link : node.internal_links_span()) {
      const int input_i = r_used_inputs.first_index_of_try(internal_link->fromsock);
      const int output_i = r_used_outputs.first_index_of_try(internal_link->tosock);
      if (ELEM(-1, input_i, output_i)) {
        continue;
      }
      input_by_output_index_[output_i] = input_i;
      inputs_[input_i].usage = lf::ValueUsage::Maybe;
    }
  }

  void execute_impl(lf::Params &params, const lf::Context &UNUSED(context)) const override
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

class LazyFunctionForMultiFunctionConversion : public LazyFunction {
 private:
  const MultiFunction &fn_;
  const ValueOrFieldCPPType &from_type_;
  const ValueOrFieldCPPType &to_type_;
  const Vector<const bNodeSocket *> target_sockets_;

 public:
  LazyFunctionForMultiFunctionConversion(const MultiFunction &fn,
                                         const ValueOrFieldCPPType &from,
                                         const ValueOrFieldCPPType &to,
                                         Vector<const bNodeSocket *> &&target_sockets)
      : fn_(fn), from_type_(from), to_type_(to), target_sockets_(std::move(target_sockets))
  {
    static_name_ = "Convert";
    inputs_.append({"From", from});
    outputs_.append({"To", to});
  }

  void execute_impl(lf::Params &params, const lf::Context &UNUSED(context)) const override
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

class LazyFunctionForMultiFunctionNode : public LazyFunction {
 private:
  const bNode &node_;
  const NodeMultiFunctions::Item fn_item_;
  Vector<const ValueOrFieldCPPType *> input_types_;
  Vector<const ValueOrFieldCPPType *> output_types_;
  Vector<const bNodeSocket *> output_sockets_;

 public:
  LazyFunctionForMultiFunctionNode(const bNode &node,
                                   NodeMultiFunctions::Item fn_item,
                                   Vector<const bNodeSocket *> &r_used_inputs,
                                   Vector<const bNodeSocket *> &r_used_outputs)
      : node_(node), fn_item_(std::move(fn_item))
  {
    BLI_assert(fn_item_.fn != nullptr);
    static_name_ = node.name;
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);
    for (const lf::Input &fn_input : inputs_) {
      input_types_.append(dynamic_cast<const ValueOrFieldCPPType *>(fn_input.type));
    }
    for (const lf::Output &fn_output : outputs_) {
      output_types_.append(dynamic_cast<const ValueOrFieldCPPType *>(fn_output.type));
    }
    output_sockets_ = r_used_outputs;
  }

  void execute_impl(lf::Params &params, const lf::Context &UNUSED(context)) const override
  {
    Vector<const void *> input_values(inputs_.size());
    Vector<void *> output_values(outputs_.size());
    for (const int i : inputs_.index_range()) {
      input_values[i] = params.try_get_input_data_ptr(i);
    }
    for (const int i : outputs_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    execute_multi_function_on_value_or_field(
        *fn_item_.fn, fn_item_.owned_fn, input_types_, output_types_, input_values, output_values);
    for (const int i : outputs_.index_range()) {
      params.output_set(i);
    }
  }
};

class LazyFunctionForComplexInput : public LazyFunction {
 private:
  std::function<void(void *)> init_fn_;

 public:
  LazyFunctionForComplexInput(const CPPType &type, std::function<void(void *)> init_fn)
      : init_fn_(std::move(init_fn))
  {
    static_name_ = "Input";
    outputs_.append({"Output", type});
  }

  void execute_impl(lf::Params &params, const lf::Context &UNUSED(context)) const override
  {
    void *value = params.get_output_data_ptr(0);
    init_fn_(value);
    params.output_set(0);
  }
};

class LazyFunctionForGroupNode : public LazyFunction {
 private:
  const bNode &group_node_;
  std::optional<GeometryNodesLazyFunctionLogger> lf_logger_;
  std::optional<lf::LazyFunctionGraphExecutor> graph_executor_;

 public:
  LazyFunctionForGroupNode(const bNode &group_node,
                           Vector<const bNodeSocket *> &r_used_inputs,
                           Vector<const bNodeSocket *> &r_used_outputs)
      : group_node_(group_node)
  {
    /* Todo: No static name. */
    static_name_ = group_node.name;
    lazy_function_interface_from_node(
        group_node, r_used_inputs, r_used_outputs, inputs_, outputs_);

    bNodeTree *group_btree = reinterpret_cast<bNodeTree *>(group_node_.id);
    BLI_assert(group_btree != nullptr); /* Todo. */
    const blender::nodes::GeometryNodesLazyFunctionGraphInfo &lf_graph_info =
        blender::nodes::ensure_geometry_nodes_lazy_function_graph(*group_btree);

    Vector<const lf::OutputSocket *> graph_inputs;
    for (const lf::OutputSocket *socket : lf_graph_info.mapping.group_input_sockets) {
      if (socket != nullptr) {
        graph_inputs.append(socket);
      }
    }
    Vector<const lf::InputSocket *> graph_outputs;
    if (const bNode *group_output_bnode = group_btree->group_output_node()) {
      for (const bNodeSocket *bsocket : group_output_bnode->input_sockets().drop_back(1)) {
        const lf::Socket *socket = lf_graph_info.mapping.dummy_socket_map.lookup_default(bsocket,
                                                                                         nullptr);
        if (socket != nullptr) {
          graph_outputs.append(&socket->as_input());
        }
      }
    }

    // std::cout << lf_graph_info.graph.to_dot() << "\n";

    lf_logger_.emplace(lf_graph_info);
    graph_executor_.emplace(
        lf_graph_info.graph, std::move(graph_inputs), std::move(graph_outputs), &*lf_logger_);
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
    BLI_assert(user_data != nullptr);
    NodeGroupContextStack context_stack{
        user_data->context_stack, group_node_.name, group_node_.id->name + 2};
    GeoNodesLFUserData group_user_data = *user_data;
    group_user_data.context_stack = &context_stack;

    lf::Context group_context = context;
    group_context.user_data = &group_user_data;

    graph_executor_->execute(params, group_context);
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

static GMutablePointer get_socket_default_value(LinearAllocator<> &allocator,
                                                const bNodeSocket &bsocket)
{
  const bNodeSocketType &typeinfo = *bsocket.typeinfo;
  const CPPType *type = get_socket_cpp_type(typeinfo);
  if (type == nullptr) {
    return {};
  }
  void *buffer = allocator.allocate(type->size(), type->alignment());
  typeinfo.get_geometry_nodes_cpp_value(bsocket, buffer);
  return {type, buffer};
}

struct GeometryNodesLazyFunctionGraphBuilder {
 private:
  const bNodeTree &btree_;
  GeometryNodesLazyFunctionGraphInfo *lf_graph_info_;
  LazyFunctionGraph *lf_graph_;
  GeometryNodeLazyFunctionMapping *mapping_;
  MultiValueMap<const bNodeSocket *, lf::InputSocket *> input_socket_map_;
  Map<const bNodeSocket *, lf::OutputSocket *> output_socket_map_;
  Map<const bNodeSocket *, lf::Node *> multi_input_socket_nodes_;
  const bke::DataTypeConversions *conversions_;

  Vector<const CPPType *> group_input_types_;
  Vector<int> group_input_indices_;
  lf::DummyNode *group_input_lf_node_;

  Vector<const CPPType *> group_output_types_;
  Vector<int> group_output_indices_;

 public:
  GeometryNodesLazyFunctionGraphBuilder(const bNodeTree &btree,
                                        GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
      : btree_(btree), lf_graph_info_(&lf_graph_info)
  {
  }

  void build()
  {
    btree_.ensure_topology_cache();

    lf_graph_ = &lf_graph_info_->graph;
    mapping_ = &lf_graph_info_->mapping;
    conversions_ = &bke::get_implicit_type_conversions();

    this->prepare_node_multi_functions();
    this->prepare_group_inputs();
    this->prepare_group_outputs();
    this->build_group_input_node();
    this->handle_nodes();
    this->handle_links();
    this->add_default_inputs();
    lf_graph_->update_node_indices();

    // std::cout << lf_graph_->to_dot() << "\n";
  }

 private:
  void prepare_node_multi_functions()
  {
    lf_graph_info_->node_multi_functions = std::make_unique<NodeMultiFunctions>(btree_);
  }

  void prepare_group_inputs()
  {
    LISTBASE_FOREACH (const bNodeSocket *, interface_bsocket, &btree_.inputs) {
      const CPPType *type = get_socket_cpp_type(*interface_bsocket->typeinfo);
      if (type != nullptr) {
        const int index = group_input_types_.append_and_get_index(type);
        group_input_indices_.append(index);
      }
      else {
        group_input_indices_.append(-1);
      }
    }
  }

  void prepare_group_outputs()
  {
    LISTBASE_FOREACH (const bNodeSocket *, interface_bsocket, &btree_.outputs) {
      const CPPType *type = get_socket_cpp_type(*interface_bsocket->typeinfo);
      if (type != nullptr) {
        const int index = group_output_types_.append_and_get_index(type);
        group_output_indices_.append(index);
      }
      else {
        group_output_indices_.append(-1);
      }
    }
  }

  void build_group_input_node()
  {
    /* Create a dummy node for the group inputs. */
    group_input_lf_node_ = &lf_graph_->add_dummy({}, group_input_types_);
    for (const int group_input_index : group_input_indices_) {
      if (group_input_index == -1) {
        mapping_->group_input_sockets.append(nullptr);
      }
      else {
        mapping_->group_input_sockets.append(&group_input_lf_node_->output(group_input_index));
      }
    }
  }

  void handle_nodes()
  {
    /* Insert all nodes into the lazy function graph. */
    for (const bNode *bnode : btree_.all_nodes()) {
      const bNodeType *node_type = bnode->typeinfo;
      if (node_type == nullptr) {
        continue;
      }
      if (bnode->is_muted()) {
        this->handle_muted_node(*bnode);
        continue;
      }
      switch (node_type->type) {
        case NODE_FRAME: {
          /* Ignored. */
          break;
        }
        case NODE_REROUTE: {
          this->handle_reroute_node(*bnode);
          break;
        }
        case NODE_GROUP_INPUT: {
          this->handle_group_input_node(*bnode);
          break;
        }
        case NODE_GROUP_OUTPUT: {
          this->handle_group_output_node(*bnode);
          break;
        }
        case NODE_GROUP: {
          this->handle_group_node(*bnode);
          break;
        }
        default: {
          if (node_type->geometry_node_execute) {
            this->handle_geometry_node(*bnode);
            break;
          }
          const NodeMultiFunctions::Item &fn_item = lf_graph_info_->node_multi_functions->try_get(
              *bnode);
          if (fn_item.fn != nullptr) {
            this->handle_multi_function_node(*bnode, fn_item);
          }
          break;
        }
      }
    }
  }

  void handle_muted_node(const bNode &bnode)
  {
    Vector<const bNodeSocket *> used_inputs;
    Vector<const bNodeSocket *> used_outputs;
    auto lazy_function = std::make_unique<LazyFunctionForMutedNode>(
        bnode, used_inputs, used_outputs);
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));
    for (const int i : used_inputs.index_range()) {
      const bNodeSocket &bsocket = *used_inputs[i];
      lf::InputSocket &lf_socket = lf_node.input(i);
      input_socket_map_.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    for (const int i : used_outputs.index_range()) {
      const bNodeSocket &bsocket = *used_outputs[i];
      lf::OutputSocket &lf_socket = lf_node.output(i);
      output_socket_map_.add_new(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
  }

  void handle_reroute_node(const bNode &bnode)
  {
    const bNodeSocket &input_bsocket = bnode.input_socket(0);
    const bNodeSocket &output_bsocket = bnode.output_socket(0);
    const CPPType *type = get_socket_cpp_type(input_bsocket);
    if (type == nullptr) {
      return;
    }

    auto lazy_function = std::make_unique<LazyFunctionForRerouteNode>(*type);
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    lf::InputSocket &lf_input = lf_node.input(0);
    lf::OutputSocket &lf_output = lf_node.output(0);
    input_socket_map_.add(&input_bsocket, &lf_input);
    output_socket_map_.add_new(&output_bsocket, &lf_output);
    mapping_->bsockets_by_lf_socket_map.add(&lf_input, &input_bsocket);
    mapping_->bsockets_by_lf_socket_map.add(&lf_output, &output_bsocket);
  }

  void handle_group_input_node(const bNode &bnode)
  {
    for (const int btree_index : group_input_indices_.index_range()) {
      const int lf_index = group_input_indices_[btree_index];
      if (lf_index == -1) {
        continue;
      }
      const bNodeSocket &bsocket = bnode.output_socket(btree_index);
      lf::OutputSocket &lf_socket = group_input_lf_node_->output(lf_index);
      output_socket_map_.add_new(&bsocket, &lf_socket);
      mapping_->dummy_socket_map.add_new(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
  }

  void handle_group_output_node(const bNode &bnode)
  {
    lf::DummyNode &group_output_lf_node = lf_graph_->add_dummy(group_output_types_, {});
    for (const int btree_index : group_output_indices_.index_range()) {
      const int lf_index = group_output_indices_[btree_index];
      if (lf_index == -1) {
        continue;
      }
      const bNodeSocket &bsocket = bnode.input_socket(btree_index);
      lf::InputSocket &lf_socket = group_output_lf_node.input(lf_index);
      input_socket_map_.add(&bsocket, &lf_socket);
      mapping_->dummy_socket_map.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
  }

  void handle_group_node(const bNode &bnode)
  {
    Vector<const bNodeSocket *> used_inputs;
    Vector<const bNodeSocket *> used_outputs;
    auto lazy_function = std::make_unique<LazyFunctionForGroupNode>(
        bnode, used_inputs, used_outputs);
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));
    for (const int i : used_inputs.index_range()) {
      const bNodeSocket &bsocket = *used_inputs[i];
      BLI_assert(!bsocket.is_multi_input());
      lf::InputSocket &lf_socket = lf_node.input(i);
      input_socket_map_.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    for (const int i : used_outputs.index_range()) {
      const bNodeSocket &bsocket = *used_outputs[i];
      lf::OutputSocket &lf_socket = lf_node.output(i);
      output_socket_map_.add_new(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
  }

  void handle_geometry_node(const bNode &bnode)
  {
    Vector<const bNodeSocket *> used_inputs;
    Vector<const bNodeSocket *> used_outputs;
    auto lazy_function = std::make_unique<LazyFunctionForGeometryNode>(
        bnode, used_inputs, used_outputs);
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    for (const int i : used_inputs.index_range()) {
      const bNodeSocket &bsocket = *used_inputs[i];
      lf::InputSocket &lf_socket = lf_node.input(i);

      if (bsocket.is_multi_input()) {
        auto multi_input_lazy_function = std::make_unique<LazyFunctionForMultiInput>(bsocket);
        lf::Node &lf_multi_input_node = lf_graph_->add_function(*multi_input_lazy_function);
        lf_graph_info_->functions.append(std::move(multi_input_lazy_function));
        lf_graph_->add_link(lf_multi_input_node.output(0), lf_socket);
        multi_input_socket_nodes_.add_new(&bsocket, &lf_multi_input_node);
        for (lf::InputSocket *lf_multi_input_socket : lf_multi_input_node.inputs()) {
          mapping_->bsockets_by_lf_socket_map.add(lf_multi_input_socket, &bsocket);
        }
      }
      else {
        input_socket_map_.add(&bsocket, &lf_socket);
        mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
      }
    }
    for (const int i : used_outputs.index_range()) {
      const bNodeSocket &bsocket = *used_outputs[i];
      lf::OutputSocket &lf_socket = lf_node.output(i);
      output_socket_map_.add_new(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
  }

  void handle_multi_function_node(const bNode &bnode, const NodeMultiFunctions::Item &fn_item)
  {
    Vector<const bNodeSocket *> used_inputs;
    Vector<const bNodeSocket *> used_outputs;
    auto lazy_function = std::make_unique<LazyFunctionForMultiFunctionNode>(
        bnode, fn_item, used_inputs, used_outputs);
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    for (const int i : used_inputs.index_range()) {
      const bNodeSocket &bsocket = *used_inputs[i];
      BLI_assert(!bsocket.is_multi_input());
      lf::InputSocket &lf_socket = lf_node.input(i);
      input_socket_map_.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    for (const int i : used_outputs.index_range()) {
      const bNodeSocket &bsocket = *used_outputs[i];
      lf::OutputSocket &lf_socket = lf_node.output(i);
      output_socket_map_.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
  }

  void handle_links()
  {
    for (const auto item : output_socket_map_.items()) {
      const bNodeSocket &from_bsocket = *item.key;
      lf::OutputSocket &from_lf_socket = *item.value;
      const Span<const bNodeLink *> links_from_bsocket = from_bsocket.directly_linked_links();

      struct TypeWithLinks {
        const CPPType *type;
        Vector<const bNodeLink *> links;
      };

      Vector<TypeWithLinks> types_with_links;
      for (const bNodeLink *link : links_from_bsocket) {
        if (link->is_muted()) {
          continue;
        }
        const bNodeSocket &to_bsocket = *link->tosock;
        if (!to_bsocket.is_available()) {
          continue;
        }
        const CPPType *to_type = get_socket_cpp_type(to_bsocket);
        if (to_type == nullptr) {
          continue;
        }
        bool inserted = false;
        for (TypeWithLinks &types_with_links : types_with_links) {
          if (types_with_links.type == to_type) {
            types_with_links.links.append(link);
            inserted = true;
            break;
          }
        }
        if (inserted) {
          continue;
        }
        types_with_links.append({to_type, {link}});
      }

      for (const TypeWithLinks &type_with_links : types_with_links) {
        const CPPType &to_type = *type_with_links.type;
        const Span<const bNodeLink *> links = type_with_links.links;

        Vector<const bNodeSocket *> target_bsockets;
        for (const bNodeLink *link : links) {
          target_bsockets.append(link->tosock);
        }

        lf::OutputSocket *converted_from_lf_socket = this->insert_type_conversion_if_necessary(
            from_lf_socket, to_type, std::move(target_bsockets));

        auto make_input_link_or_set_default = [&](lf::InputSocket &to_lf_socket) {
          if (converted_from_lf_socket == nullptr) {
            const void *default_value = to_type.default_value();
            to_lf_socket.set_default_value(default_value);
          }
          else {
            lf_graph_->add_link(*converted_from_lf_socket, to_lf_socket);
          }
        };

        for (const bNodeLink *link : links) {
          const bNodeSocket &to_bsocket = *link->tosock;
          if (to_bsocket.is_multi_input()) {
            /* TODO: Cache this index on the link. */
            int link_index = 0;
            for (const bNodeLink *multi_input_link : to_bsocket.directly_linked_links()) {
              if (multi_input_link == link) {
                break;
              }
              if (!multi_input_link->is_muted()) {
                link_index++;
              }
            }
            if (to_bsocket.owner_node().is_muted()) {
              if (link_index == 0) {
                for (lf::InputSocket *to_lf_socket : input_socket_map_.lookup(&to_bsocket)) {
                  make_input_link_or_set_default(*to_lf_socket);
                }
              }
            }
            else {
              lf::Node *multi_input_lf_node = multi_input_socket_nodes_.lookup_default(&to_bsocket,
                                                                                       nullptr);
              if (multi_input_lf_node == nullptr) {
                continue;
              }
              make_input_link_or_set_default(multi_input_lf_node->input(link_index));
            }
          }
          else {
            for (lf::InputSocket *to_lf_socket : input_socket_map_.lookup(&to_bsocket)) {
              make_input_link_or_set_default(*to_lf_socket);
            }
          }
        }
      }
    }
  }

  lf::OutputSocket *insert_type_conversion_if_necessary(
      lf::OutputSocket &from_socket,
      const CPPType &to_type,
      Vector<const bNodeSocket *> &&target_sockets)
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
      if (conversions_->is_convertible(from_base_type, to_base_type)) {
        const MultiFunction &multi_fn = *conversions_->get_conversion_multi_function(
            MFDataType::ForSingle(from_base_type), MFDataType::ForSingle(to_base_type));
        auto fn = std::make_unique<LazyFunctionForMultiFunctionConversion>(
            multi_fn, *from_field_type, *to_field_type, std::move(target_sockets));
        lf::Node &conversion_node = lf_graph_->add_function(*fn);
        lf_graph_info_->functions.append(std::move(fn));
        lf_graph_->add_link(from_socket, conversion_node.input(0));
        return &conversion_node.output(0);
      }
    }
    return nullptr;
  }

  void add_default_inputs()
  {
    for (auto item : input_socket_map_.items()) {
      const bNodeSocket &bsocket = *item.key;
      const Span<lf::InputSocket *> lf_sockets = item.value;
      for (lf::InputSocket *lf_socket : lf_sockets) {
        if (lf_socket->origin() != nullptr) {
          /* Is linked already. */
          continue;
        }
        this->add_default_input(bsocket, *lf_socket);
      }
    }
  }

  void add_default_input(const bNodeSocket &input_bsocket, lf::InputSocket &input_lf_socket)
  {
    if (this->try_add_implicit_input(input_bsocket, input_lf_socket)) {
      return;
    }
    GMutablePointer value = get_socket_default_value(lf_graph_info_->allocator, input_bsocket);
    if (value.get() == nullptr) {
      /* Not possible to add a default value. */
      return;
    }
    input_lf_socket.set_default_value(value.get());
    if (!value.type()->is_trivially_destructible()) {
      lf_graph_info_->values_to_destruct.append(value);
    }
  }

  bool try_add_implicit_input(const bNodeSocket &input_bsocket, lf::InputSocket &input_lf_socket)
  {
    const bNode &bnode = input_bsocket.owner_node();
    const NodeDeclaration *node_declaration = bnode.declaration();
    if (node_declaration == nullptr) {
      return false;
    }
    const SocketDeclaration &socket_declaration =
        *node_declaration->inputs()[input_bsocket.index()];
    if (socket_declaration.input_field_type() != InputSocketFieldType::Implicit) {
      return false;
    }
    const CPPType &type = input_lf_socket.type();
    std::function<void(void *)> init_fn = this->get_implicit_input_init_function(bnode,
                                                                                 input_bsocket);
    if (!init_fn) {
      return false;
    }

    auto lazy_function = std::make_unique<LazyFunctionForComplexInput>(type, std::move(init_fn));
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));
    lf_graph_->add_link(lf_node.output(0), input_lf_socket);
    return true;
  }

  std::function<void(void *)> get_implicit_input_init_function(const bNode &bnode,
                                                               const bNodeSocket &bsocket)
  {
    const bNodeSocketType &socket_type = *bsocket.typeinfo;
    if (socket_type.type == SOCK_VECTOR) {
      if (bnode.type == GEO_NODE_SET_CURVE_HANDLES) {
        StringRef side = ((NodeGeometrySetCurveHandlePositions *)bnode.storage)->mode ==
                                 GEO_NODE_CURVE_HANDLE_LEFT ?
                             "handle_left" :
                             "handle_right";
        return [side](void *r_value) {
          new (r_value) ValueOrField<float3>(bke::AttributeFieldInput::Create<float3>(side));
        };
      }
      else if (bnode.type == GEO_NODE_EXTRUDE_MESH) {
        return [](void *r_value) {
          new (r_value)
              ValueOrField<float3>(Field<float3>(std::make_shared<bke::NormalFieldInput>()));
        };
      }
      else {
        return [](void *r_value) {
          new (r_value) ValueOrField<float3>(bke::AttributeFieldInput::Create<float3>("position"));
        };
      }
    }
    else if (socket_type.type == SOCK_INT) {
      if (ELEM(bnode.type, FN_NODE_RANDOM_VALUE, GEO_NODE_INSTANCE_ON_POINTS)) {
        return [](void *r_value) {
          new (r_value)
              ValueOrField<int>(Field<int>(std::make_shared<bke::IDAttributeFieldInput>()));
        };
      }
      else {
        return [](void *r_value) {
          new (r_value) ValueOrField<int>(Field<int>(std::make_shared<fn::IndexFieldInput>()));
        };
      }
    }
    return {};
  }
};

const GeometryNodesLazyFunctionGraphInfo &ensure_geometry_nodes_lazy_function_graph(
    const bNodeTree &btree)
{
  std::unique_ptr<GeometryNodesLazyFunctionGraphInfo> &lf_graph_info_ptr =
      btree.runtime->geometry_nodes_lazy_function_graph_info;

  if (lf_graph_info_ptr) {
    return *lf_graph_info_ptr;
  }
  std::lock_guard lock{btree.runtime->geometry_nodes_lazy_function_graph_info_mutex};
  if (lf_graph_info_ptr) {
    return *lf_graph_info_ptr;
  }

  auto lf_graph_info = std::make_unique<GeometryNodesLazyFunctionGraphInfo>();
  GeometryNodesLazyFunctionGraphBuilder builder{btree, *lf_graph_info};
  builder.build();

  lf_graph_info_ptr = std::move(lf_graph_info);
  return *lf_graph_info_ptr;
}

void GeometryNodesLazyFunctionLogger::log_socket_value(const fn::lazy_function::Context &context,
                                                       const fn::lazy_function::Socket &lf_socket,
                                                       GPointer value) const
{
  const Span<const bNodeSocket *> bsockets =
      lf_graph_info_.mapping.bsockets_by_lf_socket_map.lookup(&lf_socket);
  if (bsockets.is_empty()) {
    return;
  }

  GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
  BLI_assert(user_data != nullptr);
  geo_eval_log::GeoTreeLogger &tree_logger =
      user_data->modifier_data->eval_log->get_local_tree_logger(*user_data->context_stack);
  for (const bNodeSocket *bsocket : bsockets) {
    if (bsocket->is_input() && !bsocket->directly_linked_sockets().is_empty()) {
      continue;
    }
    const bNode &bnode = bsocket->owner_node();
    if (bnode.is_reroute()) {
      continue;
    }
    tree_logger.log_value(bsocket->owner_node(), *bsocket, value);
  }
}

}  // namespace blender::nodes
