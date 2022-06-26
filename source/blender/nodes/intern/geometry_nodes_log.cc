/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_log.hh"
#include "NOD_geometry_nodes_to_lazy_function_graph.hh"

namespace blender::nodes::geo_eval_log {

void ReducedGeoNodesTreeEvalLog::ensure_node_warnings()
{
  if (reduced_node_warnings_) {
    return;
  }
  for (GeoNodesTreeEvalLog *tree_log : tree_logs_) {
    for (const std::pair<std::string, NodeWarning> &warnings : tree_log->node_warnings) {
      this->nodes.lookup_or_add_default(warnings.first).warnings.append(warnings.second);
      this->all_warnings.append(warnings.second);
    }
    for (const ContextStackHash &child_hash : tree_log->children_hashes) {
      ReducedGeoNodesTreeEvalLog &child_reduced_log = full_log_->get_reduced_tree_log(child_hash);
      child_reduced_log.ensure_node_warnings();
      const std::optional<std::string> &group_node_name =
          child_reduced_log.tree_logs_[0]->group_node_name;
      if (group_node_name.has_value()) {
        this->nodes.lookup_or_add_default(*group_node_name)
            .warnings.extend(child_reduced_log.all_warnings);
      }
      this->all_warnings.extend(child_reduced_log.all_warnings);
    }
  }
  reduced_node_warnings_ = true;
}

GeoNodesTreeEvalLog &GeoNodesModifierEvalLog::get_local_log(const ContextStack &context_stack)
{
  Map<ContextStackHash, std::unique_ptr<GeoNodesTreeEvalLog>> &local_log_map =
      log_map_per_thread_.local();
  std::unique_ptr<GeoNodesTreeEvalLog> &tree_log_ptr = local_log_map.lookup_or_add_default(
      context_stack.hash());
  if (tree_log_ptr) {
    return *tree_log_ptr;
  }
  tree_log_ptr = std::make_unique<GeoNodesTreeEvalLog>();
  GeoNodesTreeEvalLog &tree_log = *tree_log_ptr;
  const ContextStack *parent_context_stack = context_stack.parent();
  if (parent_context_stack != nullptr) {
    tree_log.parent_hash = parent_context_stack->hash();
    GeoNodesTreeEvalLog &parent_log = this->get_local_log(*parent_context_stack);
    parent_log.children_hashes.append(context_stack.hash());
  }
  if (const NodeGroupContextStack *node_group_context_stack =
          dynamic_cast<const NodeGroupContextStack *>(&context_stack)) {
    tree_log.group_node_name.emplace(node_group_context_stack->node_name());
  }
  return tree_log;
}

ReducedGeoNodesTreeEvalLog &GeoNodesModifierEvalLog::get_reduced_tree_log(
    const ContextStackHash &context_stack_hash)
{
  ReducedGeoNodesTreeEvalLog &reduced_tree_log = *reduced_log_map_.lookup_or_add_cb(
      context_stack_hash, [&]() {
        Vector<GeoNodesTreeEvalLog *> tree_logs;
        for (Map<ContextStackHash, std::unique_ptr<GeoNodesTreeEvalLog>> &log_map :
             log_map_per_thread_) {
          std::unique_ptr<GeoNodesTreeEvalLog> *tree_log = log_map.lookup_ptr(context_stack_hash);
          if (tree_log != nullptr) {
            tree_logs.append(tree_log->get());
          }
        }
        return std::make_unique<ReducedGeoNodesTreeEvalLog>(this, std::move(tree_logs));
      });
  return reduced_tree_log;
}

}  // namespace blender::nodes::geo_eval_log
