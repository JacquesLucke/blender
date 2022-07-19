/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>
#include <mutex>

#include "BLI_multi_value_map.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "BKE_node.h"

struct bNode;
struct bNodeSocket;
struct bNodeTree;
struct bNodeType;

namespace blender::nodes {
struct FieldInferencingInterface;
class NodeDeclaration;
}  // namespace blender::nodes

namespace blender::bke {

class bNodeTreeRuntime : NonCopyable, NonMovable {
 public:
  /**
   * Keeps track of what changed in the node tree until the next update.
   * Should not be changed directly, instead use the functions in `BKE_node_tree_update.h`.
   * #eNodeTreeChangedFlag.
   */
  uint32_t changed_flag = 0;
  /**
   * A hash of the topology of the node tree leading up to the outputs. This is used to determine
   * of the node tree changed in a way that requires updating geometry nodes or shaders.
   */
  uint32_t output_topology_hash = 0;

  /**
   * Used to cache run-time information of the node tree.
   * #eNodeTreeRuntimeFlag.
   */
  uint8_t runtime_flag = 0;

  /** Information about how inputs and outputs of the node group interact with fields. */
  std::unique_ptr<nodes::FieldInferencingInterface> field_inferencing_interface;

  std::mutex topology_cache_mutex;
  bool topology_cache_is_dirty = true;

  /** Only valid when #topology_cache_is_dirty is false. */
  Vector<bNode *> nodes;
  Vector<bNodeLink *> links;
  Vector<bNodeSocket *> sockets;
  Vector<bNodeSocket *> input_sockets;
  Vector<bNodeSocket *> output_sockets;
  MultiValueMap<const bNodeType *, bNode *> nodes_by_type;
  Vector<bNode *> toposort_left_to_right;
  Vector<bNode *> toposort_right_to_left;
  bool has_link_cycle = false;
  bool has_undefined_nodes_or_sockets = false;
  bNode *group_output_node = nullptr;
};

/**
 * Run-time data for every socket. This should only contain data that is somewhat persistent (i.e.
 * data that lives longer than a single depsgraph evaluation + redraw). Data that's only used in
 * smaller scopes should generally be stored in separate arrays and/or maps.
 */
class bNodeSocketRuntime : NonCopyable, NonMovable {
 public:
  /**
   * References a socket declaration that is owned by `node->declaration`. This is only runtime
   * data. It has to be updated when the node declaration changes.
   */
  const SocketDeclarationHandle *declaration = nullptr;

  /** #eNodeTreeChangedFlag. */
  uint32_t changed_flag = 0;

  /** Only valid when #topology_cache_is_dirty is false. */
  Vector<bNodeLink *> directly_linked_links;
  Vector<bNodeSocket *> directly_linked_sockets;
  Vector<bNodeSocket *> logically_linked_sockets;
  Vector<bNodeSocket *> logically_linked_skipped_sockets;
  bNode *owner_node = nullptr;
  bNodeSocket *internal_link_input = nullptr;
  int index_in_node = -1;
  int index_in_all_sockets = -1;
  int index_in_inout_sockets = -1;
};

/**
 * Run-time data for every node. This should only contain data that is somewhat persistent (i.e.
 * data that lives longer than a single depsgraph evaluation + redraw). Data that's only used in
 * smaller scopes should generally be stored in separate arrays and/or maps.
 */
class bNodeRuntime : NonCopyable, NonMovable {
 public:
  /**
   * Describes the desired interface of the node. This is run-time data only.
   * The actual interface of the node may deviate from the declaration temporarily.
   * It's possible to sync the actual state of the node to the desired state. Currently, this is
   * only done when a node is created or loaded.
   *
   * In the future, we may want to keep more data only in the declaration, so that it does not have
   * to be synced to other places that are stored in files. That especially applies to data that
   * can't be edited by users directly (e.g. min/max values of sockets, tooltips, ...).
   *
   * The declaration of a node can be recreated at any time when it is used. Caching it here is
   * just a bit more efficient when it is used a lot. To make sure that the cache is up-to-date,
   * call #nodeDeclarationEnsure before using it.
   *
   * Currently, the declaration is the same for every node of the same type. Going forward, that is
   * intended to change though. Especially when nodes become more dynamic with respect to how many
   * sockets they have.
   */
  NodeDeclarationHandle *declaration = nullptr;

  /** #eNodeTreeChangedFlag. */
  uint32_t changed_flag = 0;

  /** Only valid if #topology_cache_is_dirty is false. */
  Vector<bNodeSocket *> inputs;
  Vector<bNodeSocket *> outputs;
  Vector<bNodeLink *> internal_links;
  Map<StringRefNull, bNodeSocket *> inputs_by_identifier;
  Map<StringRefNull, bNodeSocket *> outputs_by_identifier;
  int index_in_tree = -1;
  bool has_linked_inputs = false;
  bool has_linked_outputs = false;
  bool is_group_node = false;
  bNodeTree *owner_tree = nullptr;
};

namespace node_tree_runtime {
void ensure_topology_cache(const bNodeTree &ntree);
}  // namespace node_tree_runtime

namespace node {

inline bool topology_cache_is_available(const bNode &node)
{
  return !node.runtime->owner_tree->runtime->topology_cache_is_dirty;
}

inline bool topology_cache_is_available(const bNodeSocket &socket)
{
  return !socket.runtime->owner_node->runtime->owner_tree->runtime->topology_cache_is_dirty;
}

inline bool topology_cache_is_available(const bNodeTree &tree)
{
  return !tree.runtime->topology_cache_is_dirty;
}

inline Span<bNodeSocket *> node_inputs(bNode &node)
{
  BLI_assert(topology_cache_is_available(node));
  return node.runtime->inputs;
}

inline Span<const bNodeSocket *> node_inputs(const bNode &node)
{
  BLI_assert(topology_cache_is_available(node));
  return node.runtime->inputs;
}

inline bNodeSocket &node_input(bNode &node, const int index)
{
  BLI_assert(topology_cache_is_available(node));
  return *node.runtime->inputs[index];
}

inline const bNodeSocket &node_input(const bNode &node, const int index)
{
  BLI_assert(topology_cache_is_available(node));
  return *node.runtime->inputs[index];
}

inline Span<bNodeSocket *> node_outputs(bNode &node)
{
  BLI_assert(topology_cache_is_available(node));
  return node.runtime->outputs;
}

inline Span<const bNodeSocket *> node_outputs(const bNode &node)
{
  BLI_assert(topology_cache_is_available(node));
  return node.runtime->outputs;
}

inline bNodeSocket &node_output(bNode &node, const int index)
{
  BLI_assert(topology_cache_is_available(node));
  return *node.runtime->outputs[index];
}

inline const bNodeSocket &node_output(const bNode &node, const int index)
{
  BLI_assert(topology_cache_is_available(node));
  return *node.runtime->outputs[index];
}

inline int socket_index_in_node(const bNodeSocket &socket)
{
  BLI_assert(topology_cache_is_available(socket));
  return socket.runtime->index_in_node;
}

inline int socket_index_in_all(const bNodeSocket &socket)
{
  BLI_assert(topology_cache_is_available(socket));
  return socket.runtime->index_in_all_sockets;
}

inline bNode &socket_owner_node(bNodeSocket &socket)
{
  BLI_assert(topology_cache_is_available(socket));
  return *socket.runtime->owner_node;
}

inline const bNode &socket_owner_node(const bNodeSocket &socket)
{
  BLI_assert(topology_cache_is_available(socket));
  return *socket.runtime->owner_node;
}

inline bool is_group_node(const bNode &node)
{
  BLI_assert(topology_cache_is_available(node));
  return node.runtime->is_group_node;
}

inline Span<const bNode *> tree_nodes(const bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->nodes;
}

inline Span<bNode *> tree_nodes(bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->nodes;
}

inline bool tree_has_link_cycle(const bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->has_link_cycle;
}

inline bool tree_has_undefined_nodes_or_sockets(const bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->has_undefined_nodes_or_sockets;
}

inline Span<bNode *> nodes_by_type(bNodeTree &tree, const StringRefNull name)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->nodes_by_type.lookup(nodeTypeFind(name.c_str()));
}

inline Span<const bNode *> nodes_by_type(const bNodeTree &tree, const StringRefNull name)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->nodes_by_type.lookup(nodeTypeFind(name.c_str()));
}

inline Span<const bNodeSocket *> logically_linked_sockets(const bNodeSocket &socket)
{
  BLI_assert(topology_cache_is_available(socket));
  return socket.runtime->logically_linked_sockets;
}

inline Span<const bNodeLink *> directly_linked_links(const bNodeSocket &socket)
{
  BLI_assert(topology_cache_is_available(socket));
  return socket.runtime->directly_linked_links;
}

inline Span<bNodeLink *> directly_linked_links(bNodeSocket &socket)
{
  BLI_assert(topology_cache_is_available(socket));
  return socket.runtime->directly_linked_links;
}

inline Span<const bNodeSocket *> directly_linked_sockets(const bNodeSocket &socket)
{
  BLI_assert(topology_cache_is_available(socket));
  return socket.runtime->directly_linked_sockets;
}

inline Span<const bNodeLink *> internal_links(const bNode &node)
{
  BLI_assert(topology_cache_is_available(node));
  return node.runtime->internal_links;
}

inline const bNodeSocket *internal_link_input(const bNodeSocket &socket)
{
  BLI_assert(topology_cache_is_available(socket));
  BLI_assert(socket.in_out == SOCK_OUT);
  return socket.runtime->internal_link_input;
}

inline const bNode *group_output_node(const bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->group_output_node;
}

inline const nodes::NodeDeclaration *node_declaration(const bNode &node)
{
  BLI_assert(node.runtime->declaration != nullptr);
  return node.runtime->declaration;
}

inline Span<const bNode *> toposort_left_to_right(const bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->toposort_left_to_right;
}

inline Span<const bNode *> toposort_right_to_left(const bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->toposort_right_to_left;
}

inline Span<const bNodeSocket *> all_inputs_in_tree(const bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->input_sockets;
}

inline Span<bNodeSocket *> all_inputs_in_tree(bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->input_sockets;
}

inline Span<const bNodeSocket *> all_outputs_in_tree(const bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->output_sockets;
}

inline Span<bNodeSocket *> all_outputs_in_tree(bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->output_sockets;
}

inline Span<const bNodeSocket *> all_sockets_in_tree(const bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->sockets;
}

inline Span<bNodeSocket *> all_sockets_in_tree(bNodeTree &tree)
{
  BLI_assert(topology_cache_is_available(tree));
  return tree.runtime->sockets;
}

}  // namespace node

}  // namespace blender::bke
