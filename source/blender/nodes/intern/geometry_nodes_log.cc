/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_log.hh"

namespace blender::nodes::geo_eval_log {

void ReducedGeoNodesTreeEvalLog::ensure_node_warnings()
{
  if (reduced_node_warnings_) {
    return;
  }
  for (GeoNodesTreeEvalLog *tree_log : tree_logs_) {
    for (const std::pair<std::string, NodeWarning> &warnings : tree_log->node_warnings) {
      this->nodes.lookup_or_add_default(warnings.first).warnings.append(warnings.second);
    }
  }
  reduced_node_warnings_ = true;
}

GeoNodesTreeEvalLog &GeoNodesModifierEvalLog::get_local_log(const ContextStack &context_stack)
{
  Map<ContextStackHash, std::unique_ptr<GeoNodesTreeEvalLog>> &local_log_map =
      log_map_per_thread_.local();
  std::unique_ptr<GeoNodesTreeEvalLog> &tree_log = local_log_map.lookup_or_add_default(
      context_stack.hash());
  if (tree_log) {
    return *tree_log;
  }
  tree_log = std::make_unique<GeoNodesTreeEvalLog>();
  const ContextStack *parent_context_stack = context_stack.parent();
  if (parent_context_stack != nullptr) {
    tree_log->parent_hash = parent_context_stack->hash();
    GeoNodesTreeEvalLog &parent_log = this->get_local_log(*parent_context_stack);
    parent_log.children_hashes.append(context_stack.hash());
  }
  return *tree_log;
}

ReducedGeoNodesTreeEvalLog &GeoNodesModifierEvalLog::get_reduced_tree_log(
    const ContextStack &context_stack)
{
  ReducedGeoNodesTreeEvalLog &reduced_tree_log = reduced_log_map_.lookup_or_add_cb(
      context_stack.hash(), [&]() {
        Vector<GeoNodesTreeEvalLog *> tree_logs;
        for (Map<ContextStackHash, std::unique_ptr<GeoNodesTreeEvalLog>> &log_map :
             log_map_per_thread_) {
          std::unique_ptr<GeoNodesTreeEvalLog> *tree_log = log_map.lookup_ptr(
              context_stack.hash());
          if (tree_log != nullptr) {
            tree_logs.append(tree_log->get());
          }
        }
        return ReducedGeoNodesTreeEvalLog{this, std::move(tree_logs)};
      });
  return reduced_tree_log;
}

}  // namespace blender::nodes::geo_eval_log
