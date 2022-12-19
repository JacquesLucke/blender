/* SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * This file mainly converts a #bNodeTree into a lazy-function graph. This generally works by
 * creating a lazy-function for every node, which is then put into the lazy-function graph. Then
 * the nodes in the new graph are linked based on links in the original #bNodeTree. Some additional
 * nodes are inserted for things like type conversions and multi-input sockets.
 *
 * Currently, lazy-functions are even created for nodes that don't strictly require it, like
 * reroutes or muted nodes. In the future we could avoid that at the cost of additional code
 * complexity. So far, this does not seem to be a performance issue.
 */

#include "NOD_geometry_exec.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_multi_function.hh"
#include "NOD_node_declaration.hh"

#include "BLI_cpp_types.hh"
#include "BLI_dot_export.hh"
#include "BLI_hash.h"
#include "BLI_lazy_threading.hh"
#include "BLI_map.hh"

#include "DNA_ID.h"

#include "BKE_compute_contexts.hh"
#include "BKE_geometry_set.hh"
#include "BKE_type_conversions.hh"

#include "FN_field_cpp_type.hh"
#include "FN_lazy_function_graph_executor.hh"

#include "DEG_depsgraph_query.h"

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
  BLI_assert(type->has_special_member_functions());
  return type;
}

static const CPPType *get_socket_cpp_type(const bNodeSocket &socket)
{
  return get_socket_cpp_type(*socket.typeinfo);
}

static const CPPType *get_vector_type(const CPPType &type)
{
  const VectorCPPType *vector_type = VectorCPPType::get_from_value(type);
  if (vector_type == nullptr) {
    return nullptr;
  }
  return &vector_type->self;
}

/**
 * Checks which sockets of the node are available and creates corresponding inputs/outputs on the
 * lazy-function.
 */
static void lazy_function_interface_from_node(const bNode &node,
                                              Vector<const bNodeSocket *> &r_used_inputs,
                                              Vector<const bNodeSocket *> &r_used_outputs,
                                              Vector<lf::Input> &r_inputs,
                                              Vector<lf::Output> &r_outputs)
{
  const bool is_muted = node.is_muted();
  const bool supports_laziness = node.typeinfo->geometry_node_execute_supports_laziness ||
                                 node.is_group();
  const lf::ValueUsage input_usage = supports_laziness ? lf::ValueUsage::Maybe :
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
  Map<StringRef, int> lf_input_for_output_;

  LazyFunctionForGeometryNode(const bNode &node,
                              Vector<const bNodeSocket *> &r_used_inputs,
                              Vector<const bNodeSocket *> &r_used_outputs)
      : node_(node)
  {
    BLI_assert(node.typeinfo->geometry_node_execute != nullptr);
    debug_name_ = node.name;
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);

    const NodeDeclaration &node_decl = *node.declaration();
    if (const aal::RelationsInNode *relations = node_decl.anonymous_attribute_relations()) {
      for (const aal::AvailableOnRelation &relation : relations->available_on_relations) {
        const bNodeSocket &field_output_bsocket = node.output_socket(relation.field_output);
        if (!field_output_bsocket.is_available()) {
          continue;
        }
        const int lf_index = inputs_.append_and_get_index_as("Output Reference Required",
                                                             CPPType::get<bool>());
        lf_input_for_output_.add(field_output_bsocket.identifier, lf_index);
      }
    }
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
    BLI_assert(user_data != nullptr);

    GeoNodeExecParams geo_params{node_, params, context, lf_input_for_output_};

    geo_eval_log::TimePoint start_time = geo_eval_log::Clock::now();
    node_.typeinfo->geometry_node_execute(geo_params);
    geo_eval_log::TimePoint end_time = geo_eval_log::Clock::now();

    if (geo_eval_log::GeoModifierLog *modifier_log = user_data->modifier_data->eval_log) {
      geo_eval_log::GeoTreeLogger &tree_logger = modifier_log->get_local_tree_logger(
          *user_data->compute_context);
      tree_logger.node_execution_times.append({node_.identifier, start_time, end_time});
    }
  }

  std::string input_name(const int index) const override
  {
    for (const auto [identifier, lf_index] : lf_input_for_output_.items()) {
      if (index == lf_index) {
        return "Add '" + identifier + "'";
      }
    }
    return inputs_[index].debug_name;
  }
};

/**
 * Used to gather all inputs of a multi-input socket. A separate node is necessary because
 * multi-inputs are not supported in lazy-function graphs.
 */
class LazyFunctionForMultiInput : public LazyFunction {
 private:
  const CPPType *base_type_;

 public:
  LazyFunctionForMultiInput(const bNodeSocket &socket)
  {
    debug_name_ = "Multi Input";
    base_type_ = get_socket_cpp_type(socket);
    BLI_assert(base_type_ != nullptr);
    BLI_assert(socket.is_multi_input());
    const bNodeTree &btree = socket.owner_tree();
    for (const bNodeLink *link : socket.directly_linked_links()) {
      if (link->is_muted() || !link->fromsock->is_available() ||
          nodeIsDanglingReroute(&btree, link->fromnode)) {
        continue;
      }
      inputs_.append({"Input", *base_type_});
    }
    const CPPType *vector_type = get_vector_type(*base_type_);
    BLI_assert(vector_type != nullptr);
    outputs_.append({"Output", *vector_type});
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    /* Currently we only have multi-inputs for geometry and string sockets. This could be
     * generalized in the future. */
    base_type_->to_static_type_tag<GeometrySet, ValueOrField<std::string>>([&](auto type_tag) {
      using T = typename decltype(type_tag)::type;
      if constexpr (std::is_void_v<T>) {
        /* This type is not supported in this node for now. */
        BLI_assert_unreachable();
      }
      else {
        void *output_ptr = params.get_output_data_ptr(0);
        Vector<T> &values = *new (output_ptr) Vector<T>();
        for (const int i : inputs_.index_range()) {
          values.append(params.extract_input<T>(i));
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
    debug_name_ = "Reroute";
    inputs_.append({"Input", type});
    outputs_.append({"Output", type});
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
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

/**
 * Lazy functions for nodes whose type cannot be found. An undefined function just outputs default
 * values. It's useful to have so other parts of the conversion don't have to care about undefined
 * nodes.
 */
class LazyFunctionForUndefinedNode : public LazyFunction {
 public:
  LazyFunctionForUndefinedNode(const bNode &node, Vector<const bNodeSocket *> &r_used_outputs)
  {
    debug_name_ = "Undefined";
    Vector<const bNodeSocket *> dummy_used_inputs;
    Vector<lf::Input> dummy_inputs;
    lazy_function_interface_from_node(
        node, dummy_used_inputs, r_used_outputs, dummy_inputs, outputs_);
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    params.set_default_remaining_outputs();
  }
};

/**
 * Executes a multi-function. If all inputs are single values, the results will also be single
 * values. If any input is a field, the outputs will also be fields.
 */
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

  /* Check if any input is a field. */
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
    /* Convert all inputs into fields, so that they can be used as input in the new field. */
    Vector<GField> input_fields;
    for (const int i : input_types.index_range()) {
      const ValueOrFieldCPPType &type = *input_types[i];
      const void *value_or_field = input_values[i];
      input_fields.append(type.as_field(value_or_field));
    }

    /* Construct the new field node. */
    std::shared_ptr<fn::FieldOperation> operation;
    if (owned_fn) {
      operation = std::make_shared<fn::FieldOperation>(owned_fn, std::move(input_fields));
    }
    else {
      operation = std::make_shared<fn::FieldOperation>(fn, std::move(input_fields));
    }

    /* Store the new fields in the output. */
    for (const int i : output_types.index_range()) {
      const ValueOrFieldCPPType &type = *output_types[i];
      void *value_or_field = output_values[i];
      type.construct_from_field(value_or_field, GField{operation, i});
    }
  }
  else {
    /* In this case, the multi-function is evaluated directly. */
    MFParamsBuilder params{fn, 1};
    MFContextBuilder context;

    for (const int i : input_types.index_range()) {
      const ValueOrFieldCPPType &type = *input_types[i];
      const void *value_or_field = input_values[i];
      const void *value = type.get_value_ptr(value_or_field);
      params.add_readonly_single_input(GVArray::ForSingleRef(type.value, 1, value));
    }
    for (const int i : output_types.index_range()) {
      const ValueOrFieldCPPType &type = *output_types[i];
      void *value_or_field = output_values[i];
      type.self.default_construct(value_or_field);
      void *value = type.get_value_ptr(value_or_field);
      type.value.destruct(value);
      params.add_uninitialized_single_output(GMutableSpan{type.value, value, 1});
    }
    fn.call(IndexRange(1), params, context);
  }
}

/**
 * Behavior of muted nodes:
 * - Some inputs are forwarded to outputs without changes.
 * - Some inputs are converted to a different type which becomes the output.
 * - Some outputs are value initialized because they don't have a corresponding input.
 */
class LazyFunctionForMutedNode : public LazyFunction {
 private:
  Array<int> input_by_output_index_;

 public:
  LazyFunctionForMutedNode(const bNode &node,
                           Vector<const bNodeSocket *> &r_used_inputs,
                           Vector<const bNodeSocket *> &r_used_outputs)
  {
    debug_name_ = "Muted";
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);
    for (lf::Input &fn_input : inputs_) {
      fn_input.usage = lf::ValueUsage::Maybe;
    }

    for (lf::Input &fn_input : inputs_) {
      fn_input.usage = lf::ValueUsage::Unused;
    }

    input_by_output_index_.reinitialize(outputs_.size());
    input_by_output_index_.fill(-1);
    for (const bNodeLink *internal_link : node.internal_links()) {
      const int input_i = r_used_inputs.first_index_of_try(internal_link->fromsock);
      const int output_i = r_used_outputs.first_index_of_try(internal_link->tosock);
      if (ELEM(-1, input_i, output_i)) {
        continue;
      }
      input_by_output_index_[output_i] = input_i;
      inputs_[input_i].usage = lf::ValueUsage::Maybe;
    }
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    for (const int output_i : outputs_.index_range()) {
      if (params.output_was_set(output_i)) {
        continue;
      }
      const CPPType &output_type = *outputs_[output_i].type;
      void *output_value = params.get_output_data_ptr(output_i);
      const int input_i = input_by_output_index_[output_i];
      if (input_i == -1) {
        /* The output does not have a corresponding input. */
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
        /* Forward the value as is. */
        input_type.copy_construct(input_value, output_value);
        params.output_set(output_i);
        continue;
      }
      /* Perform a type conversion and then format the value. */
      const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
      const auto *from_type = ValueOrFieldCPPType::get_from_self(input_type);
      const auto *to_type = ValueOrFieldCPPType::get_from_self(output_type);
      if (from_type != nullptr && to_type != nullptr) {
        if (conversions.is_convertible(from_type->value, to_type->value)) {
          const MultiFunction &multi_fn = *conversions.get_conversion_multi_function(
              MFDataType::ForSingle(from_type->value), MFDataType::ForSingle(to_type->value));
          execute_multi_function_on_value_or_field(
              multi_fn, {}, {from_type}, {to_type}, {input_value}, {output_value});
        }
        params.output_set(output_i);
        continue;
      }
      /* Use a value initialization if the conversion does not work. */
      output_type.value_initialize(output_value);
      params.output_set(output_i);
    }
  }
};

/**
 * Type conversions are generally implemented as multi-functions. This node checks if the input is
 * a field or single value and outputs a field or single value respectively.
 */
class LazyFunctionForMultiFunctionConversion : public LazyFunction {
 private:
  const MultiFunction &fn_;
  const ValueOrFieldCPPType &from_type_;
  const ValueOrFieldCPPType &to_type_;

 public:
  LazyFunctionForMultiFunctionConversion(const MultiFunction &fn,
                                         const ValueOrFieldCPPType &from,
                                         const ValueOrFieldCPPType &to)
      : fn_(fn), from_type_(from), to_type_(to)
  {
    debug_name_ = "Convert";
    inputs_.append({"From", from.self});
    outputs_.append({"To", to.self});
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
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

/**
 * This lazy-function wraps nodes that are implemented as multi-function (mostly math nodes).
 */
class LazyFunctionForMultiFunctionNode : public LazyFunction {
 private:
  const NodeMultiFunctions::Item fn_item_;
  Vector<const ValueOrFieldCPPType *> input_types_;
  Vector<const ValueOrFieldCPPType *> output_types_;

 public:
  LazyFunctionForMultiFunctionNode(const bNode &node,
                                   NodeMultiFunctions::Item fn_item,
                                   Vector<const bNodeSocket *> &r_used_inputs,
                                   Vector<const bNodeSocket *> &r_used_outputs)
      : fn_item_(std::move(fn_item))
  {
    BLI_assert(fn_item_.fn != nullptr);
    debug_name_ = node.name;
    lazy_function_interface_from_node(node, r_used_inputs, r_used_outputs, inputs_, outputs_);
    for (const lf::Input &fn_input : inputs_) {
      input_types_.append(ValueOrFieldCPPType::get_from_self(*fn_input.type));
    }
    for (const lf::Output &fn_output : outputs_) {
      output_types_.append(ValueOrFieldCPPType::get_from_self(*fn_output.type));
    }
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
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

/**
 * Some sockets have non-trivial implicit inputs (e.g. the Position input of the Set Position
 * node). Those are implemented as a separate node that outputs the value.
 */
class LazyFunctionForImplicitInput : public LazyFunction {
 private:
  /**
   * The function that generates the implicit input. The passed in memory is uninitialized.
   */
  std::function<void(void *)> init_fn_;

 public:
  LazyFunctionForImplicitInput(const CPPType &type, std::function<void(void *)> init_fn)
      : init_fn_(std::move(init_fn))
  {
    debug_name_ = "Input";
    outputs_.append({"Output", type});
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    void *value = params.get_output_data_ptr(0);
    init_fn_(value);
    params.output_set(0);
  }
};

/**
 * The viewer node does not have outputs. Instead it is executed because the executor knows that it
 * has side effects. The side effect is that the inputs to the viewer are logged.
 */
class LazyFunctionForViewerNode : public LazyFunction {
 private:
  const bNode &bnode_;
  /** The field is only logged when it is linked. */
  bool use_field_input_ = true;

 public:
  LazyFunctionForViewerNode(const bNode &bnode, Vector<const bNodeSocket *> &r_used_inputs)
      : bnode_(bnode)
  {
    debug_name_ = "Viewer";
    Vector<const bNodeSocket *> dummy_used_outputs;
    lazy_function_interface_from_node(bnode, r_used_inputs, dummy_used_outputs, inputs_, outputs_);
    const Span<const bNodeLink *> links = r_used_inputs[1]->directly_linked_links();
    if (links.is_empty() || nodeIsDanglingReroute(&bnode.owner_tree(), links.first()->fromnode)) {
      use_field_input_ = false;
      r_used_inputs.pop_last();
      inputs_.pop_last();
    }
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
    BLI_assert(user_data != nullptr);
    if (user_data->modifier_data == nullptr) {
      return;
    }
    if (user_data->modifier_data->eval_log == nullptr) {
      return;
    }

    GeometrySet geometry = params.extract_input<GeometrySet>(0);
    const NodeGeometryViewer *storage = static_cast<NodeGeometryViewer *>(bnode_.storage);

    if (use_field_input_) {
      const void *value_or_field = params.try_get_input_data_ptr(1);
      BLI_assert(value_or_field != nullptr);
      const auto &value_or_field_type = *ValueOrFieldCPPType::get_from_self(*inputs_[1].type);
      GField field = value_or_field_type.as_field(value_or_field);
      const eAttrDomain domain = eAttrDomain(storage->domain);
      const StringRefNull viewer_attribute_name = ".viewer";
      if (domain == ATTR_DOMAIN_INSTANCE) {
        if (geometry.has_instances()) {
          GeometryComponent &component = geometry.get_component_for_write(
              GEO_COMPONENT_TYPE_INSTANCES);
          bke::try_capture_field_on_geometry(
              component, viewer_attribute_name, ATTR_DOMAIN_INSTANCE, field);
        }
      }
      else {
        geometry.modify_geometry_sets([&](GeometrySet &geometry) {
          for (const GeometryComponentType type : {GEO_COMPONENT_TYPE_MESH,
                                                   GEO_COMPONENT_TYPE_POINT_CLOUD,
                                                   GEO_COMPONENT_TYPE_CURVE}) {
            if (geometry.has(type)) {
              GeometryComponent &component = geometry.get_component_for_write(type);
              eAttrDomain used_domain = domain;
              if (used_domain == ATTR_DOMAIN_AUTO) {
                if (const std::optional<eAttrDomain> detected_domain =
                        bke::try_detect_field_domain(component, field)) {
                  used_domain = *detected_domain;
                }
                else {
                  used_domain = ATTR_DOMAIN_POINT;
                }
              }
              bke::try_capture_field_on_geometry(
                  component, viewer_attribute_name, used_domain, field);
            }
          }
        });
      }
    }

    geo_eval_log::GeoTreeLogger &tree_logger =
        user_data->modifier_data->eval_log->get_local_tree_logger(*user_data->compute_context);
    tree_logger.log_viewer_node(bnode_, std::move(geometry));
  }
};

/**
 * This lazy-function wraps a group node. Internally it just executes the lazy-function graph of
 * the referenced group.
 */
class LazyFunctionForGroupNode : public LazyFunction {
 private:
  const bNode &group_node_;
  bool has_many_nodes_ = false;
  std::optional<GeometryNodesLazyFunctionLogger> lf_logger_;
  std::optional<GeometryNodesLazyFunctionSideEffectProvider> lf_side_effect_provider_;
  std::optional<lf::GraphExecutor> graph_executor_;

 public:
  Map<int, int> lf_output_by_bsocket_input_;
  Map<int, int> lf_input_by_bsocket_output_;

  LazyFunctionForGroupNode(const bNode &group_node,
                           const GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
      : group_node_(group_node)
  {
    debug_name_ = group_node.name;

    Vector<const bNodeSocket *> tmp_inputs;
    Vector<const bNodeSocket *> tmp_outputs;
    lazy_function_interface_from_node(group_node, tmp_inputs, tmp_outputs, inputs_, outputs_);

    has_many_nodes_ = lf_graph_info.num_inline_nodes_approximate > 1000;

    Vector<const lf::OutputSocket *> graph_inputs;
    graph_inputs.extend(lf_graph_info.mapping.group_input_sockets);
    for (const int i : group_node.output_sockets().index_range()) {
      lf_input_by_bsocket_output_.add_new(
          i,
          graph_inputs.append_and_get_index(lf_graph_info.mapping.group_output_used_sockets[i]));
      inputs_.append_as("Output is Used", CPPType::get<bool>(), lf::ValueUsage::Maybe);
    }
    graph_inputs.extend(lf_graph_info.mapping.group_output_used_sockets);
    Vector<const lf::InputSocket *> graph_outputs;
    graph_outputs.extend(lf_graph_info.mapping.standard_group_output_sockets);
    for (const int i : group_node.input_sockets().index_range()) {
      const InputUsage &input_usage = lf_graph_info.mapping.group_input_used_sockets[i];
      if (input_usage.type == InputUsageType::DynamicSocket) {
        lf_output_by_bsocket_input_.add_new(
            i, graph_outputs.append_and_get_index(input_usage.socket));
        outputs_.append_as("Input is Used", CPPType::get<bool>());
      }
    }

    lf_logger_.emplace(lf_graph_info);
    lf_side_effect_provider_.emplace();
    graph_executor_.emplace(lf_graph_info.graph,
                            std::move(graph_inputs),
                            std::move(graph_outputs),
                            &*lf_logger_,
                            &*lf_side_effect_provider_);
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
    BLI_assert(user_data != nullptr);

    if (has_many_nodes_) {
      /* If the called node group has many nodes, it's likely that executing it takes a while even
       * if every individual node is very small. */
      lazy_threading::send_hint();
    }

    /* The compute context changes when entering a node group. */
    bke::NodeGroupComputeContext compute_context{user_data->compute_context,
                                                 group_node_.identifier};
    GeoNodesLFUserData group_user_data = *user_data;
    group_user_data.compute_context = &compute_context;

    lf::Context group_context = context;
    group_context.user_data = &group_user_data;

    graph_executor_->execute(params, group_context);
  }

  void *init_storage(LinearAllocator<> &allocator) const override
  {
    return graph_executor_->init_storage(allocator);
  }

  void destruct_storage(void *storage) const override
  {
    graph_executor_->destruct_storage(storage);
  }

  std::string name() const override
  {
    std::stringstream ss;
    ss << "Group '" << (group_node_.id->name + 2) << "'";
    return ss.str();
  }

  std::string input_name(const int i) const override
  {
    if (i < group_node_.input_sockets().size()) {
      return group_node_.input_socket(i).name;
    }
    for (const auto [bsocket_index, lf_socket_index] : lf_input_by_bsocket_output_.items()) {
      if (i == lf_socket_index) {
        std::stringstream ss;
        ss << "'" << group_node_.output_socket(bsocket_index).name << "' output is used";
        return ss.str();
      }
    }
    BLI_assert_unreachable();
    return "";
  }

  std::string output_name(const int i) const override
  {
    if (i < group_node_.output_sockets().size()) {
      return group_node_.output_socket(i).name;
    }
    for (const auto [bsocket_index, lf_socket_index] : lf_output_by_bsocket_input_.items()) {
      if (i == lf_socket_index) {
        std::stringstream ss;
        ss << "'" << group_node_.input_socket(bsocket_index).name << "' input is used";
        return ss.str();
      }
    }
    BLI_assert_unreachable();
    return "";
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

class GroupInputDebugInfo : public lf::DummyDebugInfo {
 public:
  Vector<StringRef> socket_names;

  std::string node_name() const override
  {
    return "Group Input";
  }

  std::string output_name(const int i) const override
  {
    return this->socket_names[i];
  }
};

class GroupOutputDebugInfo : public lf::DummyDebugInfo {
 public:
  Vector<StringRef> socket_names;

  std::string node_name() const
  {
    return "Group Output";
  }

  std::string input_name(const int i) const override
  {
    return this->socket_names[i];
  }
};

class OutputIsUsedDebugInfo : public lf::DummyDebugInfo {
 public:
  std::string name;

  std::string node_name() const override
  {
    return "Output Is Used";
  }

  std::string output_name(const int /*i*/) const override
  {
    return this->name;
  }
};

class InputIsUsedDebugInfo : public lf::DummyDebugInfo {
 public:
  std::string name;

  std::string node_name() const override
  {
    return "Input Is Used";
  }

  std::string input_name(const int /*i*/) const override
  {
    return this->name;
  }
};

class LazyFunctionForLogicalOr : public lf::LazyFunction {
 public:
  LazyFunctionForLogicalOr(const int inputs_num)
  {
    debug_name_ = "Logical Or";
    for ([[maybe_unused]] const int i : IndexRange(inputs_num)) {
      inputs_.append_as("Input", CPPType::get<bool>(), lf::ValueUsage::Maybe);
    }
    outputs_.append_as("Output", CPPType::get<bool>());
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    int first_unavailable_input = -1;
    for (const int i : inputs_.index_range()) {
      if (const bool *value = params.try_get_input_data_ptr<bool>(i)) {
        if (*value) {
          params.set_output(0, true);
          return;
        }
      }
      else {
        first_unavailable_input = i;
      }
    }
    if (first_unavailable_input == -1) {
      params.set_output(0, false);
      return;
    }
    params.try_get_input_data_ptr_or_request(first_unavailable_input);
  }
};

class LazyFunctionForLogicalAnd : public lf::LazyFunction {
 public:
  LazyFunctionForLogicalAnd(const int inputs_num)
  {
    debug_name_ = "Logical And";
    for ([[maybe_unused]] const int i : IndexRange(inputs_num)) {
      inputs_.append_as("Input", CPPType::get<bool>(), lf::ValueUsage::Maybe);
    }
    outputs_.append_as("Output", CPPType::get<bool>());
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    int first_unavailable_input = -1;
    for (const int i : inputs_.index_range()) {
      if (const bool *value = params.try_get_input_data_ptr<bool>(i)) {
        if (!*value) {
          params.set_output(0, false);
          return;
        }
      }
      else {
        first_unavailable_input = i;
      }
    }
    if (first_unavailable_input == -1) {
      params.set_output(0, true);
      return;
    }
    params.try_get_input_data_ptr_or_request(first_unavailable_input);
  }
};

class LazyFunctionForLogicalNot : public lf::LazyFunction {
 public:
  LazyFunctionForLogicalNot()
  {
    debug_name_ = "Logical Not";
    inputs_.append_as("Input", CPPType::get<bool>());
    outputs_.append_as("Output", CPPType::get<bool>());
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    const bool value = params.get_input<bool>(0);
    params.set_output(0, !value);
  }
};

class LazyFunctionForSwitchSocketUsage : public lf::LazyFunction {
 public:
  LazyFunctionForSwitchSocketUsage()
  {
    debug_name_ = "Switch Socket Usage";
    inputs_.append_as("Condition", CPPType::get<ValueOrField<bool>>());
    outputs_.append_as("False", CPPType::get<bool>());
    outputs_.append_as("True", CPPType::get<bool>());
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    const ValueOrField<bool> &condition = params.get_input<ValueOrField<bool>>(0);
    if (condition.is_field()) {
      params.set_output(0, true);
      params.set_output(1, true);
    }
    else {
      const bool value = condition.as_value();
      params.set_output(0, !value);
      params.set_output(1, value);
    }
  }
};

/**
 * Utility class to build a lazy-function graph based on a geometry nodes tree.
 * This is mainly a separate class because it makes it easier to have variables that can be
 * accessed by many functions.
 */
struct GeometryNodesLazyFunctionGraphBuilder {
 private:
  const bNodeTree &btree_;
  GeometryNodesLazyFunctionGraphInfo *lf_graph_info_;
  lf::Graph *lf_graph_;
  GeometryNodeLazyFunctionGraphMapping *mapping_;
  MultiValueMap<const bNodeSocket *, lf::InputSocket *> input_socket_map_;
  Map<const bNodeSocket *, lf::OutputSocket *> output_socket_map_;
  Map<const bNodeSocket *, lf::Node *> multi_input_socket_nodes_;
  const bke::DataTypeConversions *conversions_;
  Map<const bNodeSocket *, lf::OutputSocket *> socket_is_used_map_;
  Map<const bNodeSocket *, lf::InputSocket *> use_anonymous_attributes_map_;
  Set<const lf::InputSocket *> linked_anonymous_attribute_used_inputs_;

  /**
   * All group input nodes are combined into one dummy node in the lazy-function graph.
   */
  lf::DummyNode *group_input_lf_node_;

  friend class UsedSocketVisualizeOptions;

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
    this->build_group_input_node();
    if (btree_.group_output_node() == nullptr) {
      this->build_fallback_output_node();
    }
    this->handle_nodes();
    this->handle_links();
    this->add_default_inputs();

    Map<Vector<lf::OutputSocket *>, lf::OutputSocket *> or_map;
    MultiValueMap<int, lf::OutputSocket *> inputs_used_map;

    auto or_socket_usages = [&](MutableSpan<lf::OutputSocket *> usages) -> lf::OutputSocket * {
      if (usages.is_empty()) {
        return nullptr;
      }
      if (usages.size() == 1) {
        return usages[0];
      }

      std::sort(usages.begin(), usages.end());
      return or_map.lookup_or_add_cb_as(usages, [&]() {
        auto logical_or_fn = std::make_unique<LazyFunctionForLogicalOr>(usages.size());
        lf::Node &logical_or_node = lf_graph_->add_function(*logical_or_fn);
        lf_graph_info_->functions.append(std::move(logical_or_fn));

        for (const int i : usages.index_range()) {
          lf_graph_->add_link(*usages[i], logical_or_node.input(i));
        }
        return &logical_or_node.output(0);
      });
    };

    for (const int i : btree_.interface_outputs().index_range()) {
      const bNodeSocket &interface_bsocket = *btree_.interface_outputs()[i];
      auto debug_info = std::make_unique<OutputIsUsedDebugInfo>();
      debug_info->name = interface_bsocket.name;
      lf::DummyNode &node = lf_graph_->add_dummy({}, {&CPPType::get<bool>()}, debug_info.get());
      lf_graph_info_->dummy_debug_infos_.append(std::move(debug_info));
      mapping_->group_output_used_sockets.append(&node.output(0));
    }

    for (const bNode *bnode : btree_.toposort_right_to_left()) {
      const bNodeType *node_type = bnode->typeinfo;
      /* TODO: Handle case when node type is null. */

      for (const bNodeSocket *socket : bnode->output_sockets()) {
        if (!socket->is_available()) {
          continue;
        }
        Vector<lf::OutputSocket *> target_usages;
        for (const bNodeLink *link : socket->directly_linked_links()) {
          if (link->is_muted()) {
            continue;
          }
          const bNodeSocket *target_socket = link->tosock;
          if (!target_socket->is_available()) {
            continue;
          }
          if (lf::OutputSocket *is_used_socket = socket_is_used_map_.lookup_default(target_socket,
                                                                                    nullptr)) {
            target_usages.append_non_duplicates(is_used_socket);
          }
        }
        if (lf::OutputSocket *usage = or_socket_usages(target_usages)) {
          socket_is_used_map_.add_new(socket, usage);
        }
      }

      switch (node_type->type) {
        case NODE_FRAME: {
          /* Ignored. */
          break;
        }
        case NODE_REROUTE: {
          if (lf::OutputSocket *is_used_socket = socket_is_used_map_.lookup_default(
                  &bnode->output_socket(0), nullptr)) {
            socket_is_used_map_.add_new(&bnode->input_socket(0), is_used_socket);
          }
          break;
        }
        case NODE_GROUP_OUTPUT: {
          for (const bNodeSocket *bsocket : bnode->input_sockets().drop_back(1)) {
            const int index = bsocket->index();
            socket_is_used_map_.add_new(
                bsocket,
                const_cast<lf::OutputSocket *>(mapping_->group_output_used_sockets[index]));
          }
          break;
        }
        case NODE_GROUP_INPUT: {
          for (const bNodeSocket *bsocket : bnode->output_sockets().drop_back(1)) {
            if (lf::OutputSocket *lf_socket = socket_is_used_map_.lookup_default(bsocket,
                                                                                 nullptr)) {
              const Span<lf::OutputSocket *> previous_lf_sockets = inputs_used_map.lookup(
                  bsocket->index());
              if (!previous_lf_sockets.contains(lf_socket)) {
                inputs_used_map.add(bsocket->index(), lf_socket);
              }
            }
          }
          break;
        }
        case GEO_NODE_SWITCH: {
          const bNodeSocket *switch_input_bsocket;
          const bNodeSocket *false_input_bsocket;
          const bNodeSocket *true_input_bsocket;
          const bNodeSocket *output_bsocket;
          for (const bNodeSocket *socket : bnode->input_sockets()) {
            if (!socket->is_available()) {
              continue;
            }
            if (socket->name == StringRef("Switch")) {
              switch_input_bsocket = socket;
            }
            else if (socket->name == StringRef("False")) {
              false_input_bsocket = socket;
            }
            else if (socket->name == StringRef("True")) {
              true_input_bsocket = socket;
            }
          }
          for (const bNodeSocket *socket : bnode->output_sockets()) {
            if (socket->is_available()) {
              output_bsocket = socket;
              break;
            }
          }
          if (lf::OutputSocket *output_is_used_socket = socket_is_used_map_.lookup_default(
                  output_bsocket, nullptr)) {
            socket_is_used_map_.add_new(switch_input_bsocket, output_is_used_socket);
            lf::InputSocket *lf_switch_input = input_socket_map_.lookup(switch_input_bsocket)[0];
            if (lf::OutputSocket *lf_switch_origin = lf_switch_input->origin()) {
              static const LazyFunctionForSwitchSocketUsage switch_socket_usage_fn;
              lf::Node &lf_node = lf_graph_->add_function(switch_socket_usage_fn);
              lf_graph_->add_link(*lf_switch_origin, lf_node.input(0));
              socket_is_used_map_.add_new(false_input_bsocket, &lf_node.output(0));
              socket_is_used_map_.add_new(true_input_bsocket, &lf_node.output(1));
            }
            else {
              if (switch_input_bsocket->default_value_typed<bNodeSocketValueBoolean>()->value) {
                socket_is_used_map_.add_new(true_input_bsocket, output_is_used_socket);
              }
              else {
                socket_is_used_map_.add(false_input_bsocket, output_is_used_socket);
              }
            }
          }
          break;
        }
        case NODE_GROUP:
        case NODE_CUSTOM_GROUP: {
          const bNodeTree *bgroup = reinterpret_cast<const bNodeTree *>(bnode->id);
          if (bgroup == nullptr) {
            break;
          }
          const GeometryNodesLazyFunctionGraphInfo *group_lf_graph_info =
              ensure_geometry_nodes_lazy_function_graph(*bgroup);
          if (group_lf_graph_info == nullptr) {
            break;
          }
          lf::FunctionNode &lf_group_node = const_cast<lf::FunctionNode &>(
              *mapping_->group_node_map.lookup(bnode));
          const auto &fn = static_cast<const LazyFunctionForGroupNode &>(lf_group_node.function());
          for (const bNodeSocket *input_bsocket : bnode->input_sockets()) {
            const int input_index = input_bsocket->index();
            const InputUsage &input_usage =
                group_lf_graph_info->mapping.group_input_used_sockets[input_index];
            switch (input_usage.type) {
              case InputUsageType::Never: {
                /* Nothing to do. */
                break;
              }
              case InputUsageType::DependsOnOutput: {
                /* TODO. */
                break;
              }
              case InputUsageType::DynamicSocket: {
                lf::OutputSocket &lf_input_is_used_socket = const_cast<lf::OutputSocket &>(
                    lf_group_node.output(fn.lf_output_by_bsocket_input_.lookup(input_index)));
                socket_is_used_map_.add_new(input_bsocket, &lf_input_is_used_socket);
                break;
              }
            }
          }
          for (const bNodeSocket *output_bsocket : bnode->output_sockets()) {
            const int output_index = output_bsocket->index();
            const int lf_input_index = fn.lf_input_by_bsocket_output_.lookup(output_index);
            lf::InputSocket &lf_socket = lf_group_node.input(lf_input_index);
            if (lf::OutputSocket *lf_output_is_used = socket_is_used_map_.lookup_default(
                    output_bsocket, nullptr)) {
              lf_graph_->add_link(*lf_output_is_used, lf_socket);
            }
            else {
              static const bool static_false = false;
              lf_socket.set_default_value(&static_false);
            }
          }
          break;
        }
        default: {
          for (const bNodeSocket *input_socket : bnode->input_sockets()) {
            if (!input_socket->is_available()) {
              continue;
            }
            Vector<lf::OutputSocket *> output_usages;
            for (const bNodeSocket *output_socket : bnode->output_sockets()) {
              if (!output_socket->is_available()) {
                continue;
              }
              if (lf::OutputSocket *is_used_socket = socket_is_used_map_.lookup_default(
                      output_socket, nullptr)) {
                output_usages.append_non_duplicates(is_used_socket);
              }
            }
            if (lf::OutputSocket *usage = or_socket_usages(output_usages)) {
              socket_is_used_map_.add_new(input_socket, usage);
            }
          }
          break;
        }
      }
    }

    for (const auto [output_bsocket, lf_input] : use_anonymous_attributes_map_.items()) {
      if (lf::OutputSocket *lf_is_used = socket_is_used_map_.lookup_default(output_bsocket,
                                                                            nullptr)) {
        lf_graph_->add_link(*lf_is_used, *lf_input);
        linked_anonymous_attribute_used_inputs_.add(lf_input);
      }
      else {
        static const bool static_false = false;
        lf_input->set_default_value(&static_false);
      }
    }

    for (const int i : btree_.interface_inputs().index_range()) {
      const bNodeSocket &interface_bsocket = *btree_.interface_inputs()[i];
      lf::OutputSocket *lf_socket = or_socket_usages(inputs_used_map.lookup(i));
      auto debug_info = std::make_unique<InputIsUsedDebugInfo>();
      debug_info->name = interface_bsocket.name;
      lf::DummyNode &node = lf_graph_->add_dummy({&CPPType::get<bool>()}, {}, debug_info.get());
      lf_graph_info_->dummy_debug_infos_.append(std::move(debug_info));
      InputUsage input_usage;
      if (lf_socket == nullptr) {
        static const bool static_false = false;
        node.input(0).set_default_value(&static_false);
        input_usage.type = InputUsageType::Never;
      }
      else {
        lf_graph_->add_link(*lf_socket, node.input(0));
        input_usage.type = InputUsageType::DynamicSocket;
        input_usage.socket = &node.input(0);
      }
      lf_graph_info_->mapping.group_input_used_sockets.append(std::move(input_usage));
    }

    {
      Set<lf::Socket *> lf_done_sockets;
      Stack<lf::Socket *> lf_sockets_to_check;
      for (lf::Node *lf_node : lf_graph_->nodes()) {
        if (lf_node->is_function()) {
          for (lf::OutputSocket *lf_socket : lf_node->outputs()) {
            if (lf_socket->targets().is_empty()) {
              lf_sockets_to_check.push(lf_socket);
            }
          }
        }
        if (lf_node->outputs().is_empty()) {
          for (lf::InputSocket *lf_socket : lf_node->inputs()) {
            lf_sockets_to_check.push(lf_socket);
          }
        }
      }
      Vector<lf::Socket *> cleared_origins;
      Vector<Vector<lf::Socket *>> lf_cycles;
      VectorSet<lf::Socket *> lf_socket_stack;
      while (!lf_sockets_to_check.is_empty()) {
        lf::Socket *lf_inout_socket = lf_sockets_to_check.peek();
        lf::Node &lf_node = lf_inout_socket->node();
        lf_socket_stack.add(lf_inout_socket);

        Vector<lf::Socket *> lf_origin_sockets;
        if (lf_inout_socket->is_input()) {
          lf::InputSocket &lf_input_socket = lf_inout_socket->as_input();
          if (lf::OutputSocket *lf_origin_socket = lf_input_socket.origin()) {
            lf_origin_sockets.append(lf_origin_socket);
          }
        }
        else {
          lf::OutputSocket &lf_output_socket = lf_inout_socket->as_output();
          if (lf_node.is_function()) {
            lf::FunctionNode &lf_function_node = static_cast<lf::FunctionNode &>(lf_node);
            const lf::LazyFunction &fn = lf_function_node.function();
            fn.possible_output_dependencies(
                lf_output_socket.index(), [&](const Span<int> input_indices) {
                  for (const int input_index : input_indices) {
                    lf_origin_sockets.append(&lf_node.input(input_index));
                  }
                });
          }
        }

        bool pushed_socket = false;
        for (lf::Socket *lf_origin_socket : lf_origin_sockets) {
          if (lf_socket_stack.contains(lf_origin_socket)) {
            const Span<lf::Socket *> cycle = lf_socket_stack.as_span().drop_front(
                lf_socket_stack.index_of(lf_origin_socket));
            lf_cycles.append(cycle);

            for (lf::Socket *lf_cycle_socket : cycle) {
              if (lf_cycle_socket->is_input() &&
                  this->is_output_is_used_socket(lf_cycle_socket->as_input())) {
                lf::InputSocket &lf_cycle_input_socket = lf_cycle_socket->as_input();
                lf_graph_->clear_origin(lf_cycle_input_socket);
                cleared_origins.append(&lf_cycle_input_socket);
                static const bool static_true = true;
                lf_cycle_input_socket.set_default_value(&static_true);
              }
            }
          }
          else if (!lf_done_sockets.contains(lf_origin_socket)) {
            lf_sockets_to_check.push(lf_origin_socket);
            pushed_socket = true;
          }
        }
        if (pushed_socket) {
          continue;
        }

        lf_done_sockets.add(lf_inout_socket);
        lf_sockets_to_check.pop();
        lf_socket_stack.pop();
      }

      std::cout << "Cycles: " << lf_cycles.size() << "\n";
      for (const Span<lf::Socket *> lf_cycle : lf_cycles) {
        std::cout << "  ";
        for (lf::Socket *lf_socket : lf_cycle) {
          std::cout << lf_socket->node().name() << ":" << lf_socket->name() << " -> ";
        }
        std::cout << "\n";
      }
      std::cout << "Cleared origins: " << cleared_origins.size() << "\n";
      for (const lf::Socket *lf_socket : cleared_origins) {
        std::cout << "  " << lf_socket->node().name() << ":" << lf_socket->name() << "\n";
      }
    }

    this->print_graph();

    lf_graph_->update_node_indices();
    lf_graph_info_->num_inline_nodes_approximate += lf_graph_->nodes().size();
  }

  bool is_output_is_used_socket(const lf::InputSocket &lf_socket) const
  {
    return lf_socket.name().find("output is used") != std::string::npos ||
           lf_socket.name().find("Add '") != std::string::npos;
  }

 private:
  void prepare_node_multi_functions()
  {
    lf_graph_info_->node_multi_functions = std::make_unique<NodeMultiFunctions>(btree_);
  }

  void build_group_input_node()
  {
    Vector<const CPPType *, 16> input_cpp_types;
    const Span<const bNodeSocket *> interface_inputs = btree_.interface_inputs();
    for (const bNodeSocket *interface_input : interface_inputs) {
      input_cpp_types.append(interface_input->typeinfo->geometry_nodes_cpp_type);
    }

    /* Create a dummy node for the group inputs. */
    auto debug_info = std::make_unique<GroupInputDebugInfo>();
    group_input_lf_node_ = &lf_graph_->add_dummy({}, input_cpp_types, debug_info.get());

    for (const int i : interface_inputs.index_range()) {
      mapping_->group_input_sockets.append(&group_input_lf_node_->output(i));
      debug_info->socket_names.append(interface_inputs[i]->name);
    }
    lf_graph_info_->dummy_debug_infos_.append(std::move(debug_info));
  }

  void build_fallback_output_node()
  {
    Vector<const CPPType *, 16> output_cpp_types;
    auto debug_info = std::make_unique<GroupOutputDebugInfo>();
    for (const bNodeSocket *interface_output : btree_.interface_outputs()) {
      output_cpp_types.append(interface_output->typeinfo->geometry_nodes_cpp_type);
      debug_info->socket_names.append(interface_output->name);
    }

    lf::Node &lf_node = lf_graph_->add_dummy(output_cpp_types, {}, debug_info.get());
    for (lf::InputSocket *lf_socket : lf_node.inputs()) {
      const CPPType &type = lf_socket->type();
      lf_socket->set_default_value(type.default_value());
    }
    mapping_->standard_group_output_sockets = lf_node.inputs();

    lf_graph_info_->dummy_debug_infos_.append(std::move(debug_info));
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
        case NODE_CUSTOM_GROUP:
        case NODE_GROUP: {
          this->handle_group_node(*bnode);
          break;
        }
        case GEO_NODE_VIEWER: {
          this->handle_viewer_node(*bnode);
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
            break;
          }
          if (node_type == &NodeTypeUndefined) {
            this->handle_undefined_node(*bnode);
            break;
          }
          /* Nodes that don't match any of the criteria above are just ignored. */
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
    for (const int i : btree_.interface_inputs().index_range()) {
      const bNodeSocket &bsocket = bnode.output_socket(i);
      lf::OutputSocket &lf_socket = group_input_lf_node_->output(i);
      output_socket_map_.add_new(&bsocket, &lf_socket);
      mapping_->dummy_socket_map.add_new(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
  }

  void handle_group_output_node(const bNode &bnode)
  {
    Vector<const CPPType *, 16> output_cpp_types;
    auto debug_info = std::make_unique<GroupOutputDebugInfo>();
    for (const bNodeSocket *interface_input : btree_.interface_outputs()) {
      output_cpp_types.append(interface_input->typeinfo->geometry_nodes_cpp_type);
      debug_info->socket_names.append(interface_input->name);
    }

    lf::DummyNode &group_output_lf_node = lf_graph_->add_dummy(
        output_cpp_types, {}, debug_info.get());

    for (const int i : group_output_lf_node.inputs().index_range()) {
      const bNodeSocket &bsocket = bnode.input_socket(i);
      lf::InputSocket &lf_socket = group_output_lf_node.input(i);
      input_socket_map_.add(&bsocket, &lf_socket);
      mapping_->dummy_socket_map.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }

    if (&bnode == btree_.group_output_node()) {
      mapping_->standard_group_output_sockets = group_output_lf_node.inputs();
    }

    lf_graph_info_->dummy_debug_infos_.append(std::move(debug_info));
  }

  void handle_group_node(const bNode &bnode)
  {
    const bNodeTree *group_btree = reinterpret_cast<bNodeTree *>(bnode.id);
    if (group_btree == nullptr) {
      return;
    }
    const GeometryNodesLazyFunctionGraphInfo *group_lf_graph_info =
        ensure_geometry_nodes_lazy_function_graph(*group_btree);
    if (group_lf_graph_info == nullptr) {
      return;
    }

    auto lazy_function = std::make_unique<LazyFunctionForGroupNode>(bnode, *group_lf_graph_info);
    lf::FunctionNode &lf_node = lf_graph_->add_function(*lazy_function);

    for (const int i : bnode.input_sockets().index_range()) {
      const bNodeSocket &bsocket = bnode.input_socket(i);
      BLI_assert(!bsocket.is_multi_input());
      lf::InputSocket &lf_socket = lf_node.input(i);
      input_socket_map_.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    for (const int i : bnode.output_sockets().index_range()) {
      const bNodeSocket &bsocket = bnode.output_socket(i);
      lf::OutputSocket &lf_socket = lf_node.output(i);
      output_socket_map_.add_new(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    mapping_->group_node_map.add(&bnode, &lf_node);
    lf_graph_info_->num_inline_nodes_approximate +=
        group_lf_graph_info->num_inline_nodes_approximate;
    static const bool static_false = false;
    for (const int i : lazy_function->lf_input_by_bsocket_output_.values()) {
      lf_node.input(i).set_default_value(&static_false);
    }
    lf_graph_info_->functions.append(std::move(lazy_function));
  }

  void handle_geometry_node(const bNode &bnode)
  {
    Vector<const bNodeSocket *> used_inputs;
    Vector<const bNodeSocket *> used_outputs;
    auto lazy_function = std::make_unique<LazyFunctionForGeometryNode>(
        bnode, used_inputs, used_outputs);
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);

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

    for (const auto [identifier, lf_input_index] : lazy_function->lf_input_for_output_.items()) {
      use_anonymous_attributes_map_.add_new(&bnode.output_by_identifier(identifier),
                                            &lf_node.input(lf_input_index));
    }

    lf_graph_info_->functions.append(std::move(lazy_function));
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

  void handle_viewer_node(const bNode &bnode)
  {
    Vector<const bNodeSocket *> used_inputs;
    auto lazy_function = std::make_unique<LazyFunctionForViewerNode>(bnode, used_inputs);
    lf::FunctionNode &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    for (const int i : used_inputs.index_range()) {
      const bNodeSocket &bsocket = *used_inputs[i];
      lf::InputSocket &lf_socket = lf_node.input(i);
      input_socket_map_.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }

    mapping_->viewer_node_map.add(&bnode, &lf_node);
  }

  void handle_undefined_node(const bNode &bnode)
  {
    Vector<const bNodeSocket *> used_outputs;
    auto lazy_function = std::make_unique<LazyFunctionForUndefinedNode>(bnode, used_outputs);
    lf::FunctionNode &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

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
      this->insert_links_from_socket(*item.key, *item.value);
    }
  }

  void insert_links_from_socket(const bNodeSocket &from_bsocket, lf::OutputSocket &from_lf_socket)
  {
    if (nodeIsDanglingReroute(&btree_, &from_bsocket.owner_node())) {
      return;
    }

    const Span<const bNodeLink *> links_from_bsocket = from_bsocket.directly_linked_links();

    struct TypeWithLinks {
      const CPPType *type;
      Vector<const bNodeLink *> links;
    };

    /* Group available target sockets by type so that they can be handled together. */
    Vector<TypeWithLinks> types_with_links;
    for (const bNodeLink *link : links_from_bsocket) {
      if (link->is_muted()) {
        continue;
      }
      if (!link->is_available()) {
        continue;
      }
      const bNodeSocket &to_bsocket = *link->tosock;
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

      lf::OutputSocket *converted_from_lf_socket = this->insert_type_conversion_if_necessary(
          from_lf_socket, to_type);

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
            if (multi_input_link->is_muted() || !multi_input_link->fromsock->is_available() ||
                nodeIsDanglingReroute(&btree_, multi_input_link->fromnode)) {
              continue;
            }
            link_index++;
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

  lf::OutputSocket *insert_type_conversion_if_necessary(lf::OutputSocket &from_socket,
                                                        const CPPType &to_type)
  {
    const CPPType &from_type = from_socket.type();
    if (from_type == to_type) {
      return &from_socket;
    }
    const auto *from_field_type = ValueOrFieldCPPType::get_from_self(from_type);
    const auto *to_field_type = ValueOrFieldCPPType::get_from_self(to_type);
    if (from_field_type != nullptr && to_field_type != nullptr) {
      if (conversions_->is_convertible(from_field_type->value, to_field_type->value)) {
        const MultiFunction &multi_fn = *conversions_->get_conversion_multi_function(
            MFDataType::ForSingle(from_field_type->value),
            MFDataType::ForSingle(to_field_type->value));
        auto fn = std::make_unique<LazyFunctionForMultiFunctionConversion>(
            multi_fn, *from_field_type, *to_field_type);
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
    const SocketDeclaration *socket_decl = input_bsocket.runtime->declaration;
    if (socket_decl == nullptr) {
      return false;
    }
    if (socket_decl->input_field_type() != InputSocketFieldType::Implicit) {
      return false;
    }
    const ImplicitInputValueFn *implicit_input_fn = socket_decl->implicit_input_fn();
    if (implicit_input_fn == nullptr) {
      return false;
    }
    std::function<void(void *)> init_fn = [&bnode, implicit_input_fn](void *r_value) {
      (*implicit_input_fn)(bnode, r_value);
    };
    const CPPType &type = input_lf_socket.type();
    auto lazy_function = std::make_unique<LazyFunctionForImplicitInput>(type, std::move(init_fn));
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));
    lf_graph_->add_link(lf_node.output(0), input_lf_socket);
    return true;
  }

  void print_graph();
};

class UsedSocketVisualizeOptions : public lf::Graph::ToDotOptions {
 private:
  const GeometryNodesLazyFunctionGraphBuilder &builder_;
  Map<const lf::Socket *, std::string> socket_font_colors_;
  Map<const lf::Socket *, std::string> socket_name_suffixes_;

 public:
  UsedSocketVisualizeOptions(const GeometryNodesLazyFunctionGraphBuilder &builder)
      : builder_(builder)
  {
    VectorSet<lf::OutputSocket *> found;
    for (const auto [bsocket, lf_used_socket] : builder_.socket_is_used_map_.items()) {
      const float hue = BLI_hash_int_01(uintptr_t(lf_used_socket));
      std::stringstream ss;
      ss.precision(3);
      ss << hue << " 0.9 0.5";
      const std::string color_str = ss.str();
      const std::string suffix = " (" + std::to_string(found.index_of_or_add(lf_used_socket)) +
                                 ")";
      socket_font_colors_.add(lf_used_socket, color_str);
      socket_name_suffixes_.add(lf_used_socket, suffix);

      if (bsocket->is_input()) {
        for (const lf::InputSocket *lf_socket : builder_.input_socket_map_.lookup(bsocket)) {
          socket_font_colors_.add(lf_socket, color_str);
          socket_name_suffixes_.add(lf_socket, suffix);
        }
      }
      else if (lf::OutputSocket *lf_socket = builder_.output_socket_map_.lookup(bsocket)) {
        socket_font_colors_.add(lf_socket, color_str);
        socket_name_suffixes_.add(lf_socket, suffix);
      }
    }
  }

  std::optional<std::string> socket_font_color(const lf::Socket &socket) const override
  {
    if (const std::string *color = socket_font_colors_.lookup_ptr(&socket)) {
      return *color;
    }
    return std::nullopt;
  }

  std::string socket_name(const lf::Socket &socket) const override
  {
    return socket.name() + socket_name_suffixes_.lookup_default(&socket, "");
  }

  void add_edge_attributes(const lf::OutputSocket & /*from*/,
                           const lf::InputSocket &to,
                           dot::DirectedEdge &dot_edge) const
  {
    if (builder_.linked_anonymous_attribute_used_inputs_.contains_as(&to)) {
      // dot_edge.attributes.set("constraint", "false");
      dot_edge.attributes.set("color", "#00000055");
    }
  }
};

void GeometryNodesLazyFunctionGraphBuilder::print_graph()
{
  UsedSocketVisualizeOptions options{*this};
  std::cout << "\n\n" << lf_graph_->to_dot(options) << "\n\n";
}

const GeometryNodesLazyFunctionGraphInfo *ensure_geometry_nodes_lazy_function_graph(
    const bNodeTree &btree)
{
  btree.ensure_topology_cache();
  if (btree.has_available_link_cycle()) {
    return nullptr;
  }
  if (const ID *id_orig = DEG_get_original_id(const_cast<ID *>(&btree.id))) {
    if (id_orig->tag & LIB_TAG_MISSING) {
      return nullptr;
    }
  }
  for (const bNodeSocket *interface_bsocket : btree.interface_inputs()) {
    if (interface_bsocket->typeinfo->geometry_nodes_cpp_type == nullptr) {
      return nullptr;
    }
  }
  for (const bNodeSocket *interface_bsocket : btree.interface_outputs()) {
    if (interface_bsocket->typeinfo->geometry_nodes_cpp_type == nullptr) {
      return nullptr;
    }
  }

  std::unique_ptr<GeometryNodesLazyFunctionGraphInfo> &lf_graph_info_ptr =
      btree.runtime->geometry_nodes_lazy_function_graph_info;

  if (lf_graph_info_ptr) {
    return lf_graph_info_ptr.get();
  }
  std::lock_guard lock{btree.runtime->geometry_nodes_lazy_function_graph_info_mutex};
  if (lf_graph_info_ptr) {
    return lf_graph_info_ptr.get();
  }

  auto lf_graph_info = std::make_unique<GeometryNodesLazyFunctionGraphInfo>();
  GeometryNodesLazyFunctionGraphBuilder builder{btree, *lf_graph_info};
  builder.build();

  lf_graph_info_ptr = std::move(lf_graph_info);
  return lf_graph_info_ptr.get();
}

GeometryNodesLazyFunctionLogger::GeometryNodesLazyFunctionLogger(
    const GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
    : lf_graph_info_(lf_graph_info)
{
}

void GeometryNodesLazyFunctionLogger::log_socket_value(
    const fn::lazy_function::Socket &lf_socket,
    const GPointer value,
    const fn::lazy_function::Context &context) const
{
  const Span<const bNodeSocket *> bsockets =
      lf_graph_info_.mapping.bsockets_by_lf_socket_map.lookup(&lf_socket);
  if (bsockets.is_empty()) {
    return;
  }

  GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
  BLI_assert(user_data != nullptr);
  if (user_data->modifier_data->eval_log == nullptr) {
    return;
  }
  geo_eval_log::GeoTreeLogger &tree_logger =
      user_data->modifier_data->eval_log->get_local_tree_logger(*user_data->compute_context);
  for (const bNodeSocket *bsocket : bsockets) {
    /* Avoid logging to some sockets when the same value will also be logged to a linked socket.
     * This reduces the number of logged values without losing information. */
    if (bsocket->is_input() && bsocket->is_directly_linked()) {
      continue;
    }
    const bNode &bnode = bsocket->owner_node();
    if (bnode.is_reroute()) {
      continue;
    }
    tree_logger.log_value(bsocket->owner_node(), *bsocket, value);
  }
}

static std::mutex dump_error_context_mutex;

void GeometryNodesLazyFunctionLogger::dump_when_outputs_are_missing(
    const lf::FunctionNode &node,
    Span<const lf::OutputSocket *> missing_sockets,
    const lf::Context &context) const
{
  std::lock_guard lock{dump_error_context_mutex};

  GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
  BLI_assert(user_data != nullptr);
  user_data->compute_context->print_stack(std::cout, node.name());
  std::cout << "Missing outputs:\n";
  for (const lf::OutputSocket *socket : missing_sockets) {
    std::cout << "  " << socket->name() << "\n";
  }
}

void GeometryNodesLazyFunctionLogger::dump_when_input_is_set_twice(
    const lf::InputSocket &target_socket,
    const lf::OutputSocket &from_socket,
    const lf::Context &context) const
{
  std::lock_guard lock{dump_error_context_mutex};

  std::stringstream ss;
  ss << from_socket.node().name() << ":" << from_socket.name() << " -> "
     << target_socket.node().name() << ":" << target_socket.name();

  GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
  BLI_assert(user_data != nullptr);
  user_data->compute_context->print_stack(std::cout, ss.str());
}

Vector<const lf::FunctionNode *> GeometryNodesLazyFunctionSideEffectProvider::
    get_nodes_with_side_effects(const lf::Context &context) const
{
  GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
  BLI_assert(user_data != nullptr);
  const ComputeContextHash &context_hash = user_data->compute_context->hash();
  const GeoNodesModifierData &modifier_data = *user_data->modifier_data;
  return modifier_data.side_effect_nodes->lookup(context_hash);
}

GeometryNodesLazyFunctionGraphInfo::GeometryNodesLazyFunctionGraphInfo() = default;
GeometryNodesLazyFunctionGraphInfo::~GeometryNodesLazyFunctionGraphInfo()
{
  for (GMutablePointer &p : this->values_to_destruct) {
    p.destruct();
  }
}

[[maybe_unused]] static void add_thread_id_debug_message(
    const GeometryNodesLazyFunctionGraphInfo &lf_graph_info,
    const lf::FunctionNode &node,
    const lf::Context &context)
{
  static std::atomic<int> thread_id_source = 0;
  static thread_local const int thread_id = thread_id_source.fetch_add(1);
  static thread_local const std::string thread_id_str = "Thread: " + std::to_string(thread_id);

  GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
  BLI_assert(user_data != nullptr);
  if (user_data->modifier_data->eval_log == nullptr) {
    return;
  }
  geo_eval_log::GeoTreeLogger &tree_logger =
      user_data->modifier_data->eval_log->get_local_tree_logger(*user_data->compute_context);

  /* Find corresponding node based on the socket mapping. */
  auto check_sockets = [&](const Span<const lf::Socket *> lf_sockets) {
    for (const lf::Socket *lf_socket : lf_sockets) {
      const Span<const bNodeSocket *> bsockets =
          lf_graph_info.mapping.bsockets_by_lf_socket_map.lookup(lf_socket);
      if (!bsockets.is_empty()) {
        const bNodeSocket &bsocket = *bsockets[0];
        const bNode &bnode = bsocket.owner_node();
        tree_logger.debug_messages.append({bnode.identifier, thread_id_str});
        return true;
      }
    }
    return false;
  };

  if (check_sockets(node.inputs().cast<const lf::Socket *>())) {
    return;
  }
  check_sockets(node.outputs().cast<const lf::Socket *>());
}

void GeometryNodesLazyFunctionLogger::log_before_node_execute(const lf::FunctionNode &node,
                                                              const lf::Params & /*params*/,
                                                              const lf::Context &context) const
{
  /* Enable this to see the threads that invoked a node. */
  if constexpr (false) {
    add_thread_id_debug_message(lf_graph_info_, node, context);
  }
}

}  // namespace blender::nodes
