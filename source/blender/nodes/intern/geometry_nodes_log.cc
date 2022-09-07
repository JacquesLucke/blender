/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_log.hh"
#include "NOD_geometry_nodes_to_lazy_function_graph.hh"

#include "BKE_context_stack.hh"
#include "BKE_curves.hh"

#include "FN_field_cpp_type.hh"

#include "DNA_modifier_types.h"
#include "DNA_space_types.h"

namespace blender::nodes::geo_eval_log {

using fn::FieldInput;
using fn::FieldInputs;

FieldInfoLog::FieldInfoLog(const GField &field) : type(field.cpp_type())
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
}

GeometryInfoLog::GeometryInfoLog(const GeometrySet &geometry_set)
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
    void *buffer = this->allocator->allocate(type.size(), type.alignment());
    type.copy_construct(value, buffer);
    store_logged_value(this->allocator->construct<GenericValueLog>(GMutablePointer{type, buffer}));
  };

  if (type.is<GeometrySet>()) {
    const GeometrySet &geometry = *value.get<GeometrySet>();
    store_logged_value(this->allocator->construct<GeometryInfoLog>(geometry));
  }
  else if (const auto *value_or_field_type = dynamic_cast<const fn::ValueOrFieldCPPType *>(
               &type)) {
    const void *value_or_field = value.get();
    const CPPType &base_type = value_or_field_type->base_type();
    if (value_or_field_type->is_field(value_or_field)) {
      const GField *field = value_or_field_type->get_field_ptr(value_or_field);
      if (field->node().depends_on_input()) {
        store_logged_value(this->allocator->construct<FieldInfoLog>(*field));
      }
      else {
        BUFFER_FOR_CPP_TYPE_VALUE(base_type, value);
        fn::evaluate_constant_field(*field, value);
        log_generic_value(base_type, value);
      }
    }
    else {
      const void *value = value_or_field_type->get_value_ptr(value_or_field);
      log_generic_value(base_type, value);
    }
  }
  else {
    log_generic_value(type, value.get());
  }
}

void GeoTreeLogger::log_viewer_node(const bNode &viewer_node,
                                    const GeometrySet &geometry,
                                    const GField &field)
{
  destruct_ptr<ViewerNodeLog> log = this->allocator->construct<ViewerNodeLog>();
  log->geometry = geometry;
  log->field = field;
  log->geometry.ensure_owns_direct_data();
  this->viewer_node_logs_.append({viewer_node.name, std::move(log)});
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

void GeoTreeLog::ensure_viewer_node_logs()
{
  if (reduced_viewer_node_logs_) {
    return;
  }
  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const std::tuple<std::string, destruct_ptr<ViewerNodeLog>> &viewer_log :
         tree_logger->viewer_node_logs_) {
      this->viewer_node_logs.add(std::get<0>(viewer_log), std::get<1>(viewer_log).get());
    }
  }
  reduced_viewer_node_logs_ = true;
}

void GeoTreeLog::ensure_existing_attributes()
{
  if (reduced_existing_attributes_) {
    return;
  }
  this->ensure_socket_values();

  Set<StringRef> names;

  auto handle_value_log = [&](const ValueLog &value_log) {
    const GeometryInfoLog *geo_log = dynamic_cast<const GeometryInfoLog *>(&value_log);
    if (geo_log == nullptr) {
      return;
    }
    for (const GeometryAttributeInfo &attribute : geo_log->attributes) {
      if (names.add(attribute.name)) {
        this->existing_attributes.append(&attribute);
      }
    }
  };

  for (const GeoNodeLog &node_log : this->nodes.values()) {
    for (const ValueLog *value_log : node_log.input_values_.values()) {
      handle_value_log(*value_log);
    }
    for (const ValueLog *value_log : node_log.output_values_.values()) {
      handle_value_log(*value_log);
    }
  }
  reduced_existing_attributes_ = true;
}

GeoTreeLogger &GeoModifierLog::get_local_tree_logger(const ContextStack &context_stack)
{
  LocalData &local_data = data_per_thread_.local();
  Map<ContextStackHash, destruct_ptr<GeoTreeLogger>> &local_tree_loggers =
      local_data.tree_logger_by_context;
  destruct_ptr<GeoTreeLogger> &tree_logger_ptr = local_tree_loggers.lookup_or_add_default(
      context_stack.hash());
  if (tree_logger_ptr) {
    return *tree_logger_ptr;
  }
  tree_logger_ptr = local_data.allocator.construct<GeoTreeLogger>();
  GeoTreeLogger &tree_logger = *tree_logger_ptr;
  tree_logger.allocator = &local_data.allocator;
  const ContextStack *parent_context_stack = context_stack.parent();
  if (parent_context_stack != nullptr) {
    tree_logger.parent_hash = parent_context_stack->hash();
    GeoTreeLogger &parent_logger = this->get_local_tree_logger(*parent_context_stack);
    parent_logger.children_hashes.append(context_stack.hash());
  }
  if (const bke::NodeGroupContextStack *node_group_context_stack =
          dynamic_cast<const bke::NodeGroupContextStack *>(&context_stack)) {
    tree_logger.group_node_name.emplace(node_group_context_stack->node_name());
  }
  return tree_logger;
}

GeoTreeLog &GeoModifierLog::get_tree_log(const ContextStackHash &context_stack_hash)
{
  GeoTreeLog &reduced_tree_log = *tree_logs_.lookup_or_add_cb(context_stack_hash, [&]() {
    Vector<GeoTreeLogger *> tree_logs;
    for (LocalData &local_data : data_per_thread_) {
      destruct_ptr<GeoTreeLogger> *tree_log = local_data.tree_logger_by_context.lookup_ptr(
          context_stack_hash);
      if (tree_log != nullptr) {
        tree_logs.append(tree_log->get());
      }
    }
    return std::make_unique<GeoTreeLog>(this, std::move(tree_logs));
  });
  return reduced_tree_log;
}

std::optional<GeoModifierLog::ObjectAndModifier> GeoModifierLog::get_modifier_for_node_editor(
    const SpaceNode &snode)
{
  if (snode.id == nullptr) {
    return std::nullopt;
  }
  if (GS(snode.id->name) != ID_OB) {
    return std::nullopt;
  }
  const Object *object = reinterpret_cast<Object *>(snode.id);
  const NodesModifierData *used_modifier = nullptr;
  if (snode.flag & SNODE_PIN) {
    LISTBASE_FOREACH (const ModifierData *, md, &object->modifiers) {
      if (md->type == eModifierType_Nodes) {
        const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
        /* Would be good to store the name of the pinned modifier in the node editor. */
        if (nmd->node_group == snode.nodetree) {
          used_modifier = nmd;
          break;
        }
      }
    }
  }
  else {
    LISTBASE_FOREACH (const ModifierData *, md, &object->modifiers) {
      if (md->type == eModifierType_Nodes) {
        const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
        if (nmd->node_group == snode.nodetree) {
          if (md->flag & eModifierFlag_Active) {
            used_modifier = nmd;
            break;
          }
        }
      }
    }
  }
  if (used_modifier == nullptr) {
    return std::nullopt;
  }
  return ObjectAndModifier{object, used_modifier};
}

GeoTreeLog *GeoModifierLog::get_tree_log_for_node_editor(const SpaceNode &snode)
{
  std::optional<ObjectAndModifier> object_and_modifier = get_modifier_for_node_editor(snode);
  if (!object_and_modifier) {
    return nullptr;
  }
  GeoModifierLog *modifier_log = static_cast<GeoModifierLog *>(
      object_and_modifier->nmd->runtime_eval_log);
  if (modifier_log == nullptr) {
    return nullptr;
  }
  Vector<const bNodeTreePath *> tree_path = snode.treepath;
  if (tree_path.is_empty()) {
    return nullptr;
  }
  ContextStackBuilder context_stack_builder;
  context_stack_builder.push<bke::ModifierContextStack>(object_and_modifier->nmd->modifier.name);
  for (const bNodeTreePath *path_item : tree_path.as_span().drop_front(1)) {
    context_stack_builder.push<bke::NodeGroupContextStack>(path_item->node_name);
  }
  return &modifier_log->get_tree_log(context_stack_builder.hash());
}

}  // namespace blender::nodes::geo_eval_log
