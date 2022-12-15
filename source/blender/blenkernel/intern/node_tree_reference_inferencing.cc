/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node.h"
#include "BKE_node_runtime.hh"

#include "NOD_node_declaration.hh"

#include "BLI_set.hh"
#include "BLI_stack.hh"

namespace blender::bke::node_reference_inferencing {

using nodes::NodeDeclaration;
using nodes::NodeReferenceInfo;
using nodes::SocketDeclaration;

static NodeReferenceInfo get_dummy_reference_info(const bNode &node)
{
  NodeReferenceInfo reference_info;
  reference_info.inputs.append_n_times({}, node.input_sockets().size());
  reference_info.outputs.append_n_times({}, node.output_sockets().size());
  return reference_info;
}

NodeReferenceInfo get_node_reference_info(const bNode &node)
{
  BLI_assert(!ELEM(node.type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT));
  if (node.is_group()) {
    bNodeTree *group = reinterpret_cast<bNodeTree *>(node.id);
    if (group == nullptr) {
      return {};
    }
    if (!ntreeIsRegistered(group)) {
      return get_dummy_reference_info(node);
    }
    if (!group->runtime->reference_info) {
      BLI_assert_unreachable();
      return get_dummy_reference_info(node);
    }
    return *group->runtime->reference_info;
  }
  NodeReferenceInfo reference_info;
  reference_info.inputs.append_n_times({}, node.input_sockets().size());
  reference_info.outputs.append_n_times({}, node.output_sockets().size());
  const NodeDeclaration *node_decl = node.declaration();
  if (node_decl) {
    for (const bNodeSocket *socket : node.input_sockets()) {
      const SocketDeclaration &socket_decl = *node_decl->inputs()[socket->index()];
      reference_info.inputs[socket->index()] = socket_decl.input_reference_info_;
    }
    for (const bNodeSocket *socket : node.output_sockets()) {
      const SocketDeclaration &socket_decl = *node_decl->outputs()[socket->index()];
      reference_info.outputs[socket->index()] = socket_decl.output_reference_info_;
    }
  }
  return reference_info;
}

static Vector<int> get_inputs_to_propagate_referenced_data_from(const bNodeTree &btree,
                                                                const int output_index)
{
  btree.ensure_topology_cache();
  const bNode *output_node = btree.group_output_node();
  if (output_node == nullptr) {
    return {};
  }
  const bNodeSocket &group_output_socket = output_node->input_socket(output_index);
  Set<const bNodeSocket *> pushed_sockets;
  Stack<const bNodeSocket *> sockets_to_check;
  sockets_to_check.push(&group_output_socket);
  pushed_sockets.add(&group_output_socket);

  Vector<int> indices;

  while (!sockets_to_check.is_empty()) {
    const bNodeSocket &socket = *sockets_to_check.pop();
    const bNode &node = socket.owner_node();
    if (node.is_group_input()) {
      indices.append_non_duplicates(socket.index());
      continue;
    }
    if (socket.is_input()) {
      for (const bNodeLink *link : socket.directly_linked_links()) {
        if (link->is_muted()) {
          continue;
        }
        const bNodeSocket &origin_socket = *link->fromsock;
        if (origin_socket.is_available()) {
          if (pushed_sockets.add(&origin_socket)) {
            sockets_to_check.push(&origin_socket);
          }
        }
      }
    }
    else {
      const NodeReferenceInfo reference_info = get_node_reference_info(node);
      for (const int input_index : reference_info.outputs[socket.index()].propagate_from) {
        const bNodeSocket &input_socket = node.input_socket(input_index);
        if (input_socket.is_available()) {
          if (pushed_sockets.add(&input_socket)) {
            sockets_to_check.push(&input_socket);
          }
        }
      }
    }
  }
  return indices;
}

static Vector<int> get_inputs_to_pass_references_from(const bNodeTree &btree,
                                                      const int output_index)
{
  btree.ensure_topology_cache();
  const bNode *output_node = btree.group_output_node();
  if (output_node == nullptr) {
    return {};
  }
  const bNodeSocket &group_output_socket = output_node->input_socket(output_index);
  Set<const bNodeSocket *> pushed_sockets;
  Stack<const bNodeSocket *> sockets_to_check;
  sockets_to_check.push(&group_output_socket);
  pushed_sockets.add(&group_output_socket);

  Vector<int> indices;

  while (!sockets_to_check.is_empty()) {
    const bNodeSocket &socket = *sockets_to_check.pop();
    const bNode &node = socket.owner_node();
    if (node.is_group_input()) {
      indices.append_non_duplicates(socket.index());
      continue;
    }
    if (socket.is_input()) {
      for (const bNodeLink *link : socket.directly_linked_links()) {
        if (link->is_muted()) {
          continue;
        }
        const bNodeSocket &origin_socket = *link->fromsock;
        if (origin_socket.is_available()) {
          if (pushed_sockets.add(&origin_socket)) {
            sockets_to_check.push(&origin_socket);
          }
        }
      }
    }
    else {
      const NodeReferenceInfo reference_info = get_node_reference_info(node);
      for (const int input_index : reference_info.outputs[socket.index()].pass_from) {
        const bNodeSocket &input_socket = node.input_socket(input_index);
        if (input_socket.is_available()) {
          if (pushed_sockets.add(&input_socket)) {
            sockets_to_check.push(&input_socket);
          }
        }
      }
    }
  }
  return indices;
}

static Vector<int> get_inputs_that_referenced_data_is_expected_to_be_available_on(
    const bNodeTree &btree, const int input_index)
{
  btree.ensure_topology_cache();
  const Span<const bNode *> input_nodes = btree.group_input_nodes();
  Set<const bNodeSocket *> found_sockets_where_reference_is_used;
  Stack<const bNodeSocket *> sockets_to_check;
  Set<const bNodeSocket *> pushed_sockets;
  for (const bNode *node : input_nodes) {
    const bNodeSocket &socket = node->output_socket(input_index);
    pushed_sockets.add(&socket);
    sockets_to_check.push(&socket);
  }

  while (!sockets_to_check.is_empty()) {
    const bNodeSocket &socket = *sockets_to_check.pop();
    const bNode &node = socket.owner_node();
    if (node.is_group_output()) {
      continue;
    }
    if (socket.is_input()) {
      const NodeReferenceInfo reference_info = get_node_reference_info(node);
      for (const int input_index : reference_info.inputs[socket.index()].available_on) {
        const bNodeSocket &input_socket = node.input_socket(input_index);
        if (input_socket.is_available()) {
          found_sockets_where_reference_is_used.add(&input_socket);
        }
      }
      for (const bNodeSocket *output_socket : node.output_sockets()) {
        if (!output_socket->is_available()) {
          continue;
        }
        if (reference_info.outputs[output_socket->index()].pass_from.contains(socket.index())) {
          if (pushed_sockets.add(output_socket)) {
            sockets_to_check.push(output_socket);
          }
        }
      }
    }
    else {
      for (const bNodeLink *link : socket.directly_linked_links()) {
        if (link->is_muted()) {
          continue;
        }
        const bNodeSocket &target_socket = *link->tosock;
        if (target_socket.is_available()) {
          if (pushed_sockets.add(&target_socket)) {
            sockets_to_check.push(&target_socket);
          }
        }
      }
    }
  }

  Vector<int> indices;
  pushed_sockets.clear();
  for (const bNodeSocket *socket : found_sockets_where_reference_is_used) {
    pushed_sockets.add(socket);
    sockets_to_check.push(socket);
  }
  while (!sockets_to_check.is_empty()) {
    const bNodeSocket &socket = *sockets_to_check.pop();
    const bNode &node = socket.owner_node();
    if (node.is_group_input()) {
      indices.append_non_duplicates(socket.index());
      continue;
    }
    if (socket.is_input()) {
      for (const bNodeLink *link : socket.directly_linked_links()) {
        if (link->is_muted()) {
          continue;
        }
        const bNodeSocket &origin_socket = *link->fromsock;
        if (origin_socket.is_available()) {
          if (pushed_sockets.add(&origin_socket)) {
            sockets_to_check.push(&origin_socket);
          }
        }
      }
    }
    else {
      const NodeReferenceInfo reference_info = get_node_reference_info(node);
      for (const int input_index : reference_info.outputs[socket.index()].propagate_from) {
        const bNodeSocket &input_socket = node.input_socket(input_index);
        if (input_socket.is_available()) {
          if (pushed_sockets.add(&input_socket)) {
            sockets_to_check.push(&input_socket);
          }
        }
      }
    }
  }

  return indices;
}

static std::optional<Vector<int>> get_outputs_that_referenced_data_is_available_on(
    const bNodeTree &btree, const int output_index)
{
  btree.ensure_topology_cache();
  const bNode *output_node = btree.group_output_node();
  if (output_node == nullptr) {
    return {};
  }
  const bNodeSocket &group_output_socket = output_node->input_socket(output_index);
  Set<const bNodeSocket *> found_sockets_where_reference_is_created;
  Set<const bNodeSocket *> pushed_sockets;
  Stack<const bNodeSocket *> sockets_to_check;
  sockets_to_check.push(&group_output_socket);
  pushed_sockets.add(&group_output_socket);

  while (!sockets_to_check.is_empty()) {
    const bNodeSocket &socket = *sockets_to_check.pop();
    const bNode &node = socket.owner_node();
    if (node.is_group_input()) {
      continue;
    }
    if (socket.is_input()) {
      for (const bNodeLink *link : socket.directly_linked_links()) {
        if (link->is_muted()) {
          continue;
        }
        const bNodeSocket &origin_socket = *link->fromsock;
        if (origin_socket.is_available()) {
          if (pushed_sockets.add(&origin_socket)) {
            sockets_to_check.push(&origin_socket);
          }
        }
      }
    }
    else {
      const NodeReferenceInfo reference_info = get_node_reference_info(node);
      for (const int input_index : reference_info.outputs[socket.index()].pass_from) {
        const bNodeSocket &input_socket = node.input_socket(input_index);
        if (input_socket.is_available()) {
          if (pushed_sockets.add(&input_socket)) {
            sockets_to_check.push(&input_socket);
          }
        }
      }
      if (reference_info.outputs[socket.index()].available_on.has_value()) {
        for (const int output_index : *reference_info.outputs[socket.index()].available_on) {
          const bNodeSocket &other_output = node.output_socket(output_index);
          if (other_output.is_available()) {
            found_sockets_where_reference_is_created.add(&other_output);
          }
        }
      }
    }
  }

  if (found_sockets_where_reference_is_created.is_empty()) {
    return std::nullopt;
  }

  pushed_sockets.clear();
  for (const bNodeSocket *socket : found_sockets_where_reference_is_created) {
    pushed_sockets.add(socket);
    sockets_to_check.push(socket);
  }

  Vector<int> indices;
  while (!sockets_to_check.is_empty()) {
    const bNodeSocket &socket = *sockets_to_check.pop();
    const bNode &node = socket.owner_node();
    if (node.is_group_output()) {
      indices.append_non_duplicates(socket.index());
      continue;
    }
    if (socket.is_input()) {
      const NodeReferenceInfo reference_info = get_node_reference_info(node);
      for (const bNodeSocket *output_socket : node.output_sockets()) {
        if (!output_socket->is_available()) {
          continue;
        }
        if (reference_info.outputs[output_socket->index()].propagate_from.contains(
                socket.index())) {
          if (pushed_sockets.add(output_socket)) {
            sockets_to_check.push(output_socket);
          }
        }
      }
    }
    else {
      for (const bNodeLink *link : socket.directly_linked_links()) {
        if (link->is_muted()) {
          continue;
        }
        const bNodeSocket &target_socket = *link->tosock;
        if (target_socket.is_available()) {
          if (pushed_sockets.add(&target_socket)) {
            sockets_to_check.push(&target_socket);
          }
        }
      }
    }
  }
  return indices;
}

bool update_reference_inferencing(const bNodeTree &tree)
{
  tree.ensure_topology_cache();

  std::unique_ptr<NodeReferenceInfo> new_reference_info = std::make_unique<NodeReferenceInfo>();
  new_reference_info->inputs.resize(BLI_listbase_count(&tree.inputs));
  new_reference_info->outputs.resize(BLI_listbase_count(&tree.outputs));

  for (const int input_index : new_reference_info->inputs.index_range()) {
    new_reference_info->inputs[input_index].available_on =
        get_inputs_that_referenced_data_is_expected_to_be_available_on(tree, input_index);
  }
  for (const int output_index : new_reference_info->outputs.index_range()) {
    new_reference_info->outputs[output_index].available_on =
        get_outputs_that_referenced_data_is_available_on(tree, output_index);
    new_reference_info->outputs[output_index].pass_from = get_inputs_to_pass_references_from(
        tree, output_index);
    new_reference_info->outputs[output_index].propagate_from =
        get_inputs_to_propagate_referenced_data_from(tree, output_index);
  }

  const bool group_interface_changed = !tree.runtime->reference_info ||
                                       *tree.runtime->reference_info != *new_reference_info;
  tree.runtime->reference_info = std::move(new_reference_info);

  return group_interface_changed;
}

}  // namespace blender::bke::node_reference_inferencing
