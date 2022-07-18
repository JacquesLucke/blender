/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node_runtime.hh"

#include "DNA_node_types.h"

#include "BLI_function_ref.hh"
#include "BLI_task.hh"

namespace blender::bke::node_tree_runtime {

static void double_checked_lock(std::mutex &mutex, bool &data_is_dirty, FunctionRef<void()> fn)
{
  if (!data_is_dirty) {
    return;
  }
  std::lock_guard lock{mutex};
  if (!data_is_dirty) {
    return;
  }
  fn();
  data_is_dirty = false;
}

static void double_checked_lock_with_task_isolation(std::mutex &mutex,
                                                    bool &data_is_dirty,
                                                    FunctionRef<void()> fn)
{
  double_checked_lock(mutex, data_is_dirty, [&]() { threading::isolate_task(fn); });
}

static void update_node_vector(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  tree_runtime.nodes.clear();
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    tree_runtime.nodes.append(node);
  }
}

static void update_link_vector(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  tree_runtime.links.clear();
  LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
    tree_runtime.links.append(link);
  }
}

static void update_socket_vectors(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  threading::parallel_for(tree_runtime.nodes.index_range(), 128, [&](const IndexRange range) {
    for (const int i : range) {
      bNode &node = *tree_runtime.nodes[i];
      bNodeRuntime &node_runtime = *node.runtime;
      node_runtime.inputs.clear();
      node_runtime.outputs.clear();
      LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
        node_runtime.inputs.append(socket);
      }
      LISTBASE_FOREACH (bNodeSocket *, socket, &node.outputs) {
        node_runtime.outputs.append(socket);
      }
    }
  });
}

static void update_directly_linked_links_and_sockets(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  for (bNode *node : tree_runtime.nodes) {
    for (bNodeSocket *socket : node->runtime->inputs) {
      socket->runtime->directly_linked_links.clear();
      socket->runtime->directly_linked_sockets.clear();
    }
    for (bNodeSocket *socket : node->runtime->outputs) {
      socket->runtime->directly_linked_links.clear();
      socket->runtime->directly_linked_sockets.clear();
    }
  }
  for (bNodeLink *link : tree_runtime.links) {
    link->fromsock->runtime->directly_linked_links.append(link);
    link->fromsock->runtime->directly_linked_sockets.append(link->tosock);
    link->tosock->runtime->directly_linked_links.append(link);
    link->tosock->runtime->directly_linked_sockets.append(link->fromsock);
  }
}

void ensure_topology_cache(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  double_checked_lock_with_task_isolation(
      tree_runtime.topology_cache_mutex, tree_runtime.topology_cache_is_dirty, [&]() {
        update_node_vector(ntree);
        update_link_vector(ntree);
        update_socket_vectors(ntree);
        update_directly_linked_links_and_sockets(ntree);
      });
}

}  // namespace blender::bke::node_tree_runtime
