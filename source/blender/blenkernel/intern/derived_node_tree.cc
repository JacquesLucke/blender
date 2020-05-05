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
                                                         Vector<DNode *> &r_nodes)
{
  Array<DSocket *, 64> sockets_map(tree_ref.sockets().size());

  /* Insert nodes. */
  for (const NodeRef *node_ref : tree_ref.nodes()) {
    DNode &node = this->create_node(*node_ref, parent, sockets_map);
    r_nodes.append(&node);
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

DerivedNodeTree::~DerivedNodeTree()
{
}

}  // namespace BKE
