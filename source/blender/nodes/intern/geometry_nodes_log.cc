/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_log.hh"
#include "NOD_geometry_nodes_to_lazy_function_graph.hh"

namespace blender::nodes::geo_eval_log {

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
