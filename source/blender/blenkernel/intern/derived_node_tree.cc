/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BKE_derived_node_tree.hh"

#define UNINITIALIZED_ID UINT32_MAX

namespace BKE {

static const NodeTreeRef &get_tree_ref(NodeTreeRefMap &node_tree_refs, bNodeTree *btree)
{
  return *node_tree_refs.lookup_or_add(btree,
                                       [&]() { return BLI::make_unique<NodeTreeRef>(btree); });
}

DerivedNodeTree::DerivedNodeTree(bNodeTree *btree, NodeTreeRefMap &node_tree_refs)
{
  const NodeTreeRef &main_tree_ref = get_tree_ref(node_tree_refs, btree);

  Vector<DNode *> all_nodes;

  this->insert_nodes_and_links_in_id_order(main_tree_ref, nullptr, all_nodes);
}

void DerivedNodeTree::insert_nodes_and_links_in_id_order(const NodeTreeRef &tree_ref,
                                                         DParentNode *parent,
                                                         Vector<DNode *> &all_nodes)
{
  Array<DSocket *, 64> sockets_map(tree_ref.sockets().size());

  /* Insert nodes. */
  for (const NodeRef *node_ref : tree_ref.nodes()) {
    DNode &node = this->create_node(*node_ref, parent, sockets_map);
    all_nodes.append(&node);
  }

  /* Insert links. */
  for (const NodeRef *node_ref : tree_ref.nodes()) {
    for (const InputSocketRef *to_socket_ref : node_ref->inputs()) {
      DInputSocket *to_socket = (DInputSocket *)sockets_map[to_socket_ref->id()];
      for (const OutputSocketRef *from_socket_ref : to_socket_ref->linked_sockets()) {
        DOutputSocket *from_socket = (DOutputSocket *)sockets_map[from_socket_ref->id()];
        to_socket->m_linked_sockets.append(from_socket);
        from_socket->m_linked_sockets.append(to_socket);
      }
    }
  }
}

DNode &DerivedNodeTree::create_node(const NodeRef &node_ref,
                                    DParentNode *parent,
                                    MutableArrayRef<DSocket *> r_sockets_map)
{
  DNode &node = *m_allocator.construct<DNode>();
  node.m_node_ref = &node_ref;
  node.m_parent = parent;
  node.m_id = UNINITIALIZED_ID;

  node.m_inputs = m_allocator.construct_elements_and_pointer_array<DInputSocket>(
      node_ref.inputs().size());
  node.m_outputs = m_allocator.construct_elements_and_pointer_array<DOutputSocket>(
      node_ref.outputs().size());

  for (uint i : node.m_inputs.index_range()) {
    const InputSocketRef &socket_ref = node_ref.input(i);
    DInputSocket &socket = *node.m_inputs[i];

    socket.m_id = UNINITIALIZED_ID;
    socket.m_node = &node;
    socket.m_socket_ref = &socket_ref;

    r_sockets_map[socket_ref.id()] = &socket;
  }

  for (uint i : node.m_outputs.index_range()) {
    const OutputSocketRef &socket_ref = node_ref.output(i);
    DOutputSocket &socket = *node.m_outputs[i];

    socket.m_id = UNINITIALIZED_ID;
    socket.m_node = &node;
    socket.m_socket_ref = &socket_ref;

    r_sockets_map[socket_ref.id()] = &socket;
  }

  return node;
}

void DerivedNodeTree::expand_groups(Vector<DNode *> &all_nodes,
                                    Vector<DGroupInput *> &all_group_inputs,
                                    Vector<DParentNode *> &all_parent_nodes,
                                    NodeTreeRefMap &node_tree_refs)
{
  for (uint i = 0; i < all_nodes.size(); i++) {
    DNode &node = *all_nodes[i];
    if (node.m_node_ref->is_group_node()) {
      this->expand_group_node(node, all_nodes, all_group_inputs, all_parent_nodes, node_tree_refs);
    }
  }
}

void DerivedNodeTree::expand_group_node(DNode &group_node,
                                        Vector<DNode *> &all_nodes,
                                        Vector<DGroupInput *> &all_group_inputs,
                                        Vector<DParentNode *> &all_parent_nodes,
                                        NodeTreeRefMap &node_tree_refs)
{
  const NodeRef &group_node_ref = *group_node.m_node_ref;
  BLI_assert(group_node_ref.is_group_node());

  bNodeTree *btree = (bNodeTree *)group_node_ref.bnode()->id;
  if (btree == nullptr) {
    return;
  }

  const NodeTreeRef &group_ref = get_tree_ref(node_tree_refs, btree);

  DParentNode &parent = *m_allocator.construct<DParentNode>();
  parent.m_id = all_parent_nodes.append_and_get_index(&parent);
  parent.m_parent = group_node.m_parent;
  parent.m_node_ref = &group_node_ref;

  this->insert_nodes_and_links_in_id_order(group_ref, &parent, all_nodes);
  ArrayRef<DNode *> new_nodes_by_id = all_nodes.as_ref().take_back(group_ref.nodes().size());

  this->create_group_inputs_for_unlinked_inputs(group_node, all_group_inputs);
  this->relink_group_inputs(group_ref, new_nodes_by_id, group_node);
}

void DerivedNodeTree::create_group_inputs_for_unlinked_inputs(
    DNode &node, Vector<DGroupInput *> &all_group_inputs)
{
  for (DInputSocket *input_socket : node.m_inputs) {
    if (input_socket->is_linked()) {
      continue;
    }

    DGroupInput &group_input = *m_allocator.construct<DGroupInput>();
    group_input.m_id = all_group_inputs.append_and_get_index(&group_input);
    group_input.m_socket_ref = &input_socket->socket_ref();
    group_input.m_parent = node.m_parent;

    group_input.m_linked_sockets.append(input_socket);
    input_socket->m_linked_group_inputs.append(&group_input);
  }
}

void DerivedNodeTree::relink_group_inputs(const NodeTreeRef &group_ref,
                                          ArrayRef<DNode *> nodes_by_id,
                                          DNode &group_node)
{
  ArrayRef<const NodeRef *> node_refs = group_ref.nodes_with_idname("NodeGroupInput");
  if (node_refs.size() == 0) {
    return;
  }
  /* TODO: Pick correct group input node if there are more than one. */
  const NodeRef &input_node_ref = *node_refs[0];
  DNode &input_node = *nodes_by_id[input_node_ref.id()];

  uint input_amount = group_node.inputs().size();
  BLI_assert(input_amount == input_node_ref.outputs().size() - 1);

  /* Links Before:
   *    outside_connected <----> outside_group
   *    inside_connected  <----> inside_group
   * Links After:
   *    outside_connected <----> inside_connected
   */

  for (uint input_index : IndexRange(input_amount)) {
    DInputSocket *outside_group = group_node.m_inputs[input_index];
    DOutputSocket *inside_group = input_node.m_outputs[input_index];

    for (DOutputSocket *outside_connected : outside_group->m_linked_sockets) {
      outside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(outside_group);
    }

    for (DGroupInput *outside_connected : outside_group->m_linked_group_inputs) {
      outside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(outside_group);
    }

    for (DInputSocket *inside_connected : inside_group->m_linked_sockets) {
      inside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(inside_group);

      for (DOutputSocket *outside_connected : outside_group->m_linked_sockets) {
        inside_connected->m_linked_sockets.append(outside_connected);
        outside_connected->m_linked_sockets.append(inside_connected);
      }

      for (DGroupInput *outside_connected : outside_group->m_linked_group_inputs) {
        inside_connected->m_linked_group_inputs.append(outside_connected);
        outside_connected->m_linked_sockets.append(inside_connected);
      }
    }

    inside_group->m_linked_sockets.clear();
    outside_group->m_linked_sockets.clear();
    outside_group->m_linked_group_inputs.clear();
  }
}

DerivedNodeTree::~DerivedNodeTree()
{
}

}  // namespace BKE
