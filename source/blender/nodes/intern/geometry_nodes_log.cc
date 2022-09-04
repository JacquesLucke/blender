/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_log.hh"
#include "NOD_geometry_nodes_to_lazy_function_graph.hh"

#include "BKE_curves.hh"

#include "FN_field_cpp_type.hh"

namespace blender::nodes::geo_eval_log {

using fn::FieldInput;
using fn::FieldInputs;

GFieldValueLog::GFieldValueLog(const GField &field, const bool log_full_field)
    : type(field.cpp_type())
{
  const std::shared_ptr<const fn::FieldInputs> &field_input_nodes = field.node().field_inputs();

  /* Put the deduplicated field inputs into a vector so that they can be sorted below. */
  Vector<std::reference_wrapper<const FieldInput>> field_inputs;
  if (field_input_nodes) {
    field_inputs.extend(field_input_nodes->deduplicated_nodes.begin(),
                        field_input_nodes->deduplicated_nodes.end());
  }

  std::sort(
      field_inputs.begin(), field_inputs.end(), [](const FieldInput &a, const FieldInput &b) {
        const int index_a = (int)a.category();
        const int index_b = (int)b.category();
        if (index_a == index_b) {
          return a.socket_inspection_name().size() < b.socket_inspection_name().size();
        }
        return index_a < index_b;
      });

  for (const FieldInput &field_input : field_inputs) {
    this->input_tooltips.append(field_input.socket_inspection_name());
  }

  if (log_full_field) {
    this->field = std::move(field);
  }
}

GeometryValueLog::GeometryValueLog(const GeometrySet &geometry_set, const bool log_full_geometry)
{
  static std::array all_component_types = {GEO_COMPONENT_TYPE_CURVE,
                                           GEO_COMPONENT_TYPE_INSTANCES,
                                           GEO_COMPONENT_TYPE_MESH,
                                           GEO_COMPONENT_TYPE_POINT_CLOUD,
                                           GEO_COMPONENT_TYPE_VOLUME};

  /* Keep track handled attribute names to make sure that we do not return the same name twice.
   * Currently #GeometrySet::attribute_foreach does not do that. Note that this will merge
   * attributes with the same name but different domains or data types on separate components. */
  Set<StringRef> names;

  geometry_set.attribute_foreach(
      all_component_types,
      true,
      [&](const bke::AttributeIDRef &attribute_id,
          const bke::AttributeMetaData &meta_data,
          const GeometryComponent &UNUSED(component)) {
        if (attribute_id.is_named() && names.add(attribute_id.name())) {
          this->attributes.append({attribute_id.name(), meta_data.domain, meta_data.data_type});
        }
      });

  for (const GeometryComponent *component : geometry_set.get_components_for_read()) {
    this->component_types.append(component->type());
    switch (component->type()) {
      case GEO_COMPONENT_TYPE_MESH: {
        const MeshComponent &mesh_component = *(const MeshComponent *)component;
        MeshInfo &info = this->mesh_info.emplace();
        info.verts_num = mesh_component.attribute_domain_size(ATTR_DOMAIN_POINT);
        info.edges_num = mesh_component.attribute_domain_size(ATTR_DOMAIN_EDGE);
        info.faces_num = mesh_component.attribute_domain_size(ATTR_DOMAIN_FACE);
        break;
      }
      case GEO_COMPONENT_TYPE_CURVE: {
        const CurveComponent &curve_component = *(const CurveComponent *)component;
        CurveInfo &info = this->curve_info.emplace();
        info.splines_num = curve_component.attribute_domain_size(ATTR_DOMAIN_CURVE);
        break;
      }
      case GEO_COMPONENT_TYPE_POINT_CLOUD: {
        const PointCloudComponent &pointcloud_component = *(const PointCloudComponent *)component;
        PointCloudInfo &info = this->pointcloud_info.emplace();
        info.points_num = pointcloud_component.attribute_domain_size(ATTR_DOMAIN_POINT);
        break;
      }
      case GEO_COMPONENT_TYPE_INSTANCES: {
        const InstancesComponent &instances_component = *(const InstancesComponent *)component;
        InstancesInfo &info = this->instances_info.emplace();
        info.instances_num = instances_component.instances_num();
        break;
      }
      case GEO_COMPONENT_TYPE_EDIT: {
        const GeometryComponentEditData &edit_component = *(
            const GeometryComponentEditData *)component;
        if (const bke::CurvesEditHints *curve_edit_hints =
                edit_component.curves_edit_hints_.get()) {
          EditDataInfo &info = this->edit_data_info.emplace();
          info.has_deform_matrices = curve_edit_hints->deform_mats.has_value();
          info.has_deformed_positions = curve_edit_hints->positions.has_value();
        }
        break;
      }
      case GEO_COMPONENT_TYPE_VOLUME: {
        break;
      }
    }
  }
  if (log_full_geometry) {
    this->full_geometry = std::make_unique<GeometrySet>(geometry_set);
    this->full_geometry->ensure_owns_direct_data();
  }
}

void GeoTreeLogger::log_value(const bNode &node, const bNodeSocket &socket, const GPointer value)
{
  const CPPType &type = *value.type();

  auto store_logged_value = [&](destruct_ptr<ValueLog> value_log) {
    auto &socket_values = socket.in_out == SOCK_IN ? this->input_socket_values :
                                                     this->output_socket_values;
    socket_values.append({node.name, socket.identifier, value_log.get()});
    this->socket_values_owner.append(std::move(value_log));
  };

  auto log_generic_value = [&](const CPPType &type, const void *value) {
    void *buffer = this->allocator.allocate(type.size(), type.alignment());
    type.copy_construct(value, buffer);
    store_logged_value(this->allocator.construct<GenericValueLog>(GMutablePointer{type, buffer}));
  };

  if (type.is<GeometrySet>()) {
    const GeometrySet &geometry = *value.get<GeometrySet>();
    store_logged_value(this->allocator.construct<GeometryValueLog>(geometry, false));
  }
  else if (const auto *value_or_field_type = dynamic_cast<const fn::ValueOrFieldCPPType *>(
               &type)) {
    const void *value_or_field = value.get();
    if (value_or_field_type->is_field(value_or_field)) {
      const GField *field = value_or_field_type->get_field_ptr(value_or_field);
      bool log_full_field = false;
      if (!field->node().depends_on_input()) {
        /* Always log constant fields so that their value can be shown in socket inspection.
         * In the future we can also evaluate the field here and only store the value. */
        log_full_field = true;
      }
      store_logged_value(this->allocator.construct<GFieldValueLog>(*field, log_full_field));
    }
    else {
      const CPPType &base_type = value_or_field_type->base_type();
      const void *value = value_or_field_type->get_value_ptr(value_or_field);
      log_generic_value(base_type, value);
    }
  }
  else {
    log_generic_value(type, value.get());
  }
}

void GeoTreeLog::ensure_node_warnings()
{
  if (reduced_node_warnings_) {
    return;
  }
  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const std::pair<std::string, NodeWarning> &warnings : tree_logger->node_warnings) {
      this->nodes.lookup_or_add_default(warnings.first).warnings.append(warnings.second);
      this->all_warnings.append(warnings.second);
    }
    for (const ContextStackHash &child_hash : tree_logger->children_hashes) {
      GeoTreeLog &child_reduced_log = modifier_log_->get_tree_log(child_hash);
      child_reduced_log.ensure_node_warnings();
      const std::optional<std::string> &group_node_name =
          child_reduced_log.tree_loggers_[0]->group_node_name;
      if (group_node_name.has_value()) {
        this->nodes.lookup_or_add_default(*group_node_name)
            .warnings.extend(child_reduced_log.all_warnings);
      }
      this->all_warnings.extend(child_reduced_log.all_warnings);
    }
  }
  reduced_node_warnings_ = true;
}

void GeoTreeLog::ensure_node_run_time()
{
  if (reduced_node_run_times_) {
    return;
  }
  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const std::tuple<std::string, TimePoint, TimePoint> &timings :
         tree_logger->node_execution_times) {
      const StringRefNull node_name = std::get<0>(timings);
      const std::chrono::nanoseconds duration = std::get<2>(timings) - std::get<1>(timings);
      this->nodes.lookup_or_add_default_as(node_name).run_time += duration;
      this->run_time_sum += duration;
    }
    for (const ContextStackHash &child_hash : tree_logger->children_hashes) {
      GeoTreeLog &child_reduced_log = modifier_log_->get_tree_log(child_hash);
      child_reduced_log.ensure_node_run_time();
      const std::optional<std::string> &group_node_name =
          child_reduced_log.tree_loggers_[0]->group_node_name;
      if (group_node_name.has_value()) {
        this->nodes.lookup_or_add_default(*group_node_name).run_time +=
            child_reduced_log.run_time_sum;
      }
      this->run_time_sum += child_reduced_log.run_time_sum;
    }
  }
  reduced_node_run_times_ = true;
}

void GeoTreeLog::ensure_socket_values()
{
  if (reduced_socket_values_) {
    return;
  }
  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const std::tuple<std::string, std::string, ValueLog *> &value_log_data :
         tree_logger->input_socket_values) {
      this->nodes.lookup_or_add_as(std::get<0>(value_log_data))
          .input_values_.add(std::get<1>(value_log_data), std::get<2>(value_log_data));
    }
    for (const std::tuple<std::string, std::string, ValueLog *> &value_log_data :
         tree_logger->output_socket_values) {
      this->nodes.lookup_or_add_as(std::get<0>(value_log_data))
          .output_values_.add(std::get<1>(value_log_data), std::get<2>(value_log_data));
    }
  }
  reduced_socket_values_ = true;
}

GeoTreeLogger &GeoModifierLog::get_local_tree_logger(const ContextStack &context_stack)
{
  Map<ContextStackHash, std::unique_ptr<GeoTreeLogger>> &local_tree_loggers =
      tree_loggers_per_thread_.local();
  std::unique_ptr<GeoTreeLogger> &tree_logger_ptr = local_tree_loggers.lookup_or_add_default(
      context_stack.hash());
  if (tree_logger_ptr) {
    return *tree_logger_ptr;
  }
  tree_logger_ptr = std::make_unique<GeoTreeLogger>();
  GeoTreeLogger &tree_logger = *tree_logger_ptr;
  const ContextStack *parent_context_stack = context_stack.parent();
  if (parent_context_stack != nullptr) {
    tree_logger.parent_hash = parent_context_stack->hash();
    GeoTreeLogger &parent_logger = this->get_local_tree_logger(*parent_context_stack);
    parent_logger.children_hashes.append(context_stack.hash());
  }
  if (const NodeGroupContextStack *node_group_context_stack =
          dynamic_cast<const NodeGroupContextStack *>(&context_stack)) {
    tree_logger.group_node_name.emplace(node_group_context_stack->node_name());
  }
  return tree_logger;
}

GeoTreeLog &GeoModifierLog::get_tree_log(const ContextStackHash &context_stack_hash)
{
  GeoTreeLog &reduced_tree_log = *tree_logs_.lookup_or_add_cb(context_stack_hash, [&]() {
    Vector<GeoTreeLogger *> tree_logs;
    for (Map<ContextStackHash, std::unique_ptr<GeoTreeLogger>> &log_map :
         tree_loggers_per_thread_) {
      std::unique_ptr<GeoTreeLogger> *tree_log = log_map.lookup_ptr(context_stack_hash);
      if (tree_log != nullptr) {
        tree_logs.append(tree_log->get());
      }
    }
    return std::make_unique<GeoTreeLog>(this, std::move(tree_logs));
  });
  return reduced_tree_log;
}

}  // namespace blender::nodes::geo_eval_log
