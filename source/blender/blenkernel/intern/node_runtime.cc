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
    node->runtime->index_in_tree = tree_runtime.nodes.append_and_get_index(node);
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
        socket->runtime->index_in_node = node_runtime.inputs.append_and_get_index(socket);
        socket->runtime->index_in_all_sockets = tree_runtime.sockets.append_and_get_index(socket);
        socket->runtime->index_in_inout_sockets = tree_runtime.input_sockets.append_and_get_index(
            socket);
        socket->runtime->owner_node = &node;
      }
      LISTBASE_FOREACH (bNodeSocket *, socket, &node.outputs) {
        socket->runtime->index_in_node = node_runtime.outputs.append_and_get_index(socket);
        socket->runtime->index_in_all_sockets = tree_runtime.sockets.append_and_get_index(socket);
        socket->runtime->index_in_inout_sockets = tree_runtime.output_sockets.append_and_get_index(
            socket);
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
    node->runtime->has_linked_inputs = false;
    node->runtime->has_linked_outputs = false;
  }
  for (bNodeLink *link : tree_runtime.links) {
    link->fromsock->runtime->directly_linked_links.append(link);
    link->tosock->runtime->directly_linked_links.append(link);
    link->tosock->runtime->directly_linked_sockets.append(link->fromsock);
    link->fromnode->runtime->has_linked_outputs = true;
    link->tonode->runtime->has_linked_inputs = true;
  }
  for (bNodeSocket *socket : tree_runtime.input_sockets) {
    if (socket->flag & SOCK_MULTI_INPUT) {
      std::sort(socket->runtime->directly_linked_links.begin(),
                socket->runtime->directly_linked_links.end(),
                [&](const bNodeLink *a, const bNodeLink *b) {
                  return a->multi_input_socket_index > b->multi_input_socket_index;
                });
    }
  }
  for (bNodeSocket *socket : tree_runtime.input_sockets) {
    for (bNodeLink *link : socket->runtime->directly_linked_links) {
      /* Do this after sorting the input links. */
      socket->runtime->directly_linked_sockets.append(link->tosock);
    }
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

static void update_nodes_by_type(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  tree_runtime.nodes_by_type.clear();
  for (bNode *node : tree_runtime.nodes) {
    tree_runtime.nodes_by_type.add(node->typeinfo, node);
  }
}

static void update_sockets_by_identifier(const bNodeTree &ntree)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  threading::parallel_for(tree_runtime.nodes.index_range(), 128, [&](const IndexRange range) {
    for (bNode *node : tree_runtime.nodes.as_span().slice(range)) {
      node->runtime->inputs_by_identifier.clear();
      node->runtime->outputs_by_identifier.clear();
      for (bNodeSocket *socket : node->runtime->inputs) {
        node->runtime->inputs_by_identifier.add_new(socket->identifier, socket);
      }
      for (bNodeSocket *socket : node->runtime->outputs) {
        node->runtime->outputs_by_identifier.add_new(socket->identifier, socket);
      }
    }
  });
}

enum class ToposortDirection {
  LeftToRight,
  RightToLeft,
};

struct ToposortNodeState {
  bool is_done = false;
  bool is_in_stack = false;
};

static void toposort_from_start_node(const ToposortDirection direction,
                                     bNode &start_node,
                                     MutableSpan<ToposortNodeState> node_states,
                                     Vector<bNode *> r_sorted_nodes,
                                     bool &r_cycle_detected)
{
  struct Item {
    bNode *node;
    int socket_index = 0;
    int link_index = 0;
  };

  Stack<Item, 64> nodes_to_check;
  nodes_to_check.push({&start_node});
  while (!nodes_to_check.is_empty()) {
    Item &item = nodes_to_check.peek();
    bNode &node = *item.node;
    const Span<bNodeSocket *> sockets = (direction == ToposortDirection::LeftToRight) ?
                                            node.runtime->inputs :
                                            node.runtime->outputs;
    while (true) {
      if (item.socket_index == sockets.size()) {
        /* All sockets have already been visited. */
        break;
      }
      bNodeSocket &socket = *sockets[item.socket_index];
      const Span<bNodeSocket *> linked_sockets = socket.runtime->directly_linked_sockets;
      if (item.link_index == linked_sockets.size()) {
        /* All links connected to this socket have already been visited. */
        item.socket_index++;
        item.link_index = 0;
        continue;
      }
      bNodeSocket &linked_socket = *linked_sockets[item.link_index];
      bNode &linked_node = *linked_socket.runtime->owner_node;
      ToposortNodeState &linked_node_state = node_states[linked_node.runtime->index_in_tree];
      if (linked_node_state.is_done) {
        /* The linked node has already been visited. */
        item.link_index++;
        continue;
      }
      if (linked_node_state.is_in_stack) {
        r_cycle_detected = true;
      }
      else {
        nodes_to_check.push({&linked_node});
        linked_node_state.is_in_stack = true;
      }
      break;
    }

    /* If no other element has been pushed, the current node can be pushed to the sorted list. */
    if (&item == &nodes_to_check.peek()) {
      ToposortNodeState &node_state = node_states[node.runtime->index_in_tree];
      node_state.is_done = true;
      node_state.is_in_stack = false;
      r_sorted_nodes.append(&node);
      nodes_to_check.pop();
    }
  }
}

static void update_toposort(const bNodeTree &ntree,
                            const ToposortDirection direction,
                            Vector<bNode *> &r_sorted_nodes,
                            bool &r_cycle_detected)
{
  bNodeTreeRuntime &tree_runtime = *ntree.runtime;
  r_sorted_nodes.clear();
  r_sorted_nodes.reserve(tree_runtime.nodes.size());
  r_cycle_detected = false;

  Array<ToposortNodeState> node_states(tree_runtime.nodes.size());
  for (bNode *node : tree_runtime.nodes) {
    if (node_states[node->runtime->index_in_tree].is_done) {
      /* Ignore nodes that are done already. */
      continue;
    }
    if ((direction == ToposortDirection::LeftToRight) ? node->runtime->has_linked_outputs :
                                                        node->runtime->has_linked_inputs) {
      /* Ignore non-start nodes. */
      continue;
    }
    toposort_from_start_node(direction, *node, node_states, r_sorted_nodes, r_cycle_detected);
  }

  if (r_sorted_nodes.size() < tree_runtime.nodes.size()) {
    r_cycle_detected = true;
    for (bNode *node : tree_runtime.nodes) {
      if (node_states[node->runtime->index_in_tree].is_done) {
        /* Ignore nodes that are done already. */
        continue;
      }
      /* Start toposort at this node which is somewhere in the middle of a loop. */
      toposort_from_start_node(direction, *node, node_states, r_sorted_nodes, r_cycle_detected);
    }
  }

  BLI_assert(tree_runtime.nodes.size() == r_sorted_nodes.size());
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
        threading::parallel_invoke([&]() { update_logical_origins(ntree); },
                                   [&]() { update_nodes_by_type(ntree); },
                                   [&]() { update_sockets_by_identifier(ntree); },
                                   [&]() {
                                     update_toposort(ntree,
                                                     ToposortDirection::LeftToRight,
                                                     tree_runtime.toposort_left_to_right,
                                                     tree_runtime.has_link_cycle);
                                   },
                                   [&]() {
                                     bool dummy;
                                     update_toposort(ntree,
                                                     ToposortDirection::RightToLeft,
                                                     tree_runtime.toposort_right_to_left,
                                                     dummy);
                                   });
      });
}

}  // namespace blender::bke::node_tree_runtime
