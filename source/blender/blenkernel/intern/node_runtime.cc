/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node.h"
#include "BKE_node_runtime.hh"

#include "DNA_node_types.h"

#include "BLI_function_ref.hh"
#include "BLI_stack.hh"
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

static void update_internal_links(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  for (bNode *node : tree_runtime.nodes) {
    node->runtime->internal_links.clear();
    for (bNodeSocket *socket : node->runtime->outputs) {
      socket->runtime->internal_link_input = nullptr;
    }
    LISTBASE_FOREACH (bNodeLink *, link, &node->internal_links) {
      node->runtime->internal_links.append(link);
      link->tosock->runtime->internal_link_input = link->fromsock;
    }
  }
}

static void update_socket_vectors_and_owner_node(const bNodeTree &ntree)
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
        socket->runtime->owner_node = &node;
      }
      LISTBASE_FOREACH (bNodeSocket *, socket, &node.outputs) {
        node_runtime.outputs.append(socket);
        socket->runtime->owner_node = &node;
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

static void update_logical_origins_for_socket(bNodeSocket &input_socket)
{
  bNodeSocketRuntime &socket_runtime = *input_socket.runtime;
  socket_runtime.logically_linked_sockets.clear();

  Vector<bNodeSocket *, 16> seen_sockets;
  Stack<bNodeSocket *, 16> sockets_to_check;
  sockets_to_check.push(&input_socket);

  while (!sockets_to_check.is_empty()) {
    bNodeSocket &socket = *sockets_to_check.pop();
    if (seen_sockets.contains(&socket)) {
      continue;
    }
    seen_sockets.append(&socket);
    Span<bNodeLink *> links = socket.runtime->directly_linked_links;
    if (socket_runtime.owner_node->flag & NODE_MUTED) {
      links = links.take_front(1);
    }
    for (bNodeLink *link : links) {
      if (link->flag & NODE_LINK_MUTED) {
        continue;
      }
      bNodeSocket &origin_socket = *link->fromsock;
      bNode &origin_node = *link->fromnode;
      if (origin_socket.flag & SOCK_UNAVAIL) {
        continue;
      }
      if (origin_node.type == NODE_REROUTE) {
        bNodeSocket &reroute_output = origin_socket;
        bNodeSocket &reroute_input = *origin_node.runtime->inputs[0];
        socket_runtime.logically_linked_skipped_sockets.append(&reroute_input);
        socket_runtime.logically_linked_skipped_sockets.append(&reroute_output);
        sockets_to_check.push(&reroute_input);
        continue;
      }
      if (origin_node.flag & NODE_MUTED) {
        if (bNodeSocket *mute_input = origin_socket.runtime->internal_link_input) {
          socket_runtime.logically_linked_skipped_sockets.append(&socket);
          socket_runtime.logically_linked_skipped_sockets.append(mute_input);
          sockets_to_check.push(mute_input);
        }
        continue;
      }
      socket_runtime.logically_linked_sockets.append(&socket);
    }
  }

  socket_runtime.logically_linked_sockets.remove_first_occurrence_and_reorder(&input_socket);
  socket_runtime.logically_linked_skipped_sockets.remove_first_occurrence_and_reorder(
      &input_socket);
}

static void update_logical_origins(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  threading::parallel_for(tree_runtime.nodes.index_range(), 128, [&](const IndexRange range) {
    for (const int i : range) {
      bNode &node = *tree_runtime.nodes[i];
      for (bNodeSocket *socket : node.runtime->inputs) {
        update_logical_origins_for_socket(*socket);
      }
    }
  });
}

void ensure_topology_cache(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  double_checked_lock_with_task_isolation(
      tree_runtime.topology_cache_mutex, tree_runtime.topology_cache_is_dirty, [&]() {
        update_node_vector(ntree);
        update_link_vector(ntree);
        update_internal_links(ntree);
        update_socket_vectors_and_owner_node(ntree);
        update_directly_linked_links_and_sockets(ntree);
        update_logical_origins(ntree);
      });
}

}  // namespace blender::bke::node_tree_runtime
