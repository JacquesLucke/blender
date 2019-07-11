#include "BKE_node_tree.hpp"
#include "BLI_timeit.hpp"

namespace BKE {

IndexedNodeTree::IndexedNodeTree(bNodeTree *btree)
    : m_btree(btree), m_original_nodes(btree->nodes, true), m_original_links(btree->links, true)
{
  for (bNode *bnode : m_original_nodes) {
    for (bNodeSocket *bsocket : bSocketList(bnode->inputs)) {
      m_node_by_socket.add(bsocket, bnode);
    }
    for (bNodeSocket *bsocket : bSocketList(bnode->outputs)) {
      m_node_by_socket.add(bsocket, bnode);
    }
  }

  for (bNode *bnode : m_original_nodes) {
    m_nodes_by_idname.add(bnode->idname, bnode);
    if (!this->is_reroute(bnode) && !this->is_frame(bnode)) {
      m_actual_nodes.append(bnode);
    }
  }

  for (bNodeLink *blink : m_original_links) {
    m_direct_links.add(blink->tosock, {blink->fromsock, blink->fromnode});
    m_direct_links.add(blink->fromsock, {blink->tosock, blink->tonode});
  }

  for (bNodeLink *blink : m_original_links) {
    if (!this->is_reroute(blink->fromnode) && !m_links.contains(blink->fromsock)) {
      SmallVector<SocketWithNode> connected;
      this->find_connected_sockets_right(blink->fromsock, connected);
      m_links.add_multiple_new(blink->fromsock, connected);
    }
    if (!this->is_reroute(blink->tonode) && !m_links.contains(blink->tosock)) {
      SmallVector<SocketWithNode> connected;
      this->find_connected_sockets_left(blink->tosock, connected);
      m_links.add_multiple_new(blink->tosock, connected);
      if (connected.size() == 1) {
        m_single_origin_links.append(SingleOriginLink{connected[0].socket, blink->tosock, blink});
      }
    }
  }
}

void IndexedNodeTree::find_connected_sockets_left(bNodeSocket *bsocket,
                                                  SmallVector<SocketWithNode> &r_sockets) const
{
  BLI_assert(bsocket->in_out == SOCK_IN);
  auto from_sockets = m_direct_links.lookup_default(bsocket);
  for (SocketWithNode linked : from_sockets) {
    if (this->is_reroute(linked.node)) {
      this->find_connected_sockets_left((bNodeSocket *)linked.node->inputs.first, r_sockets);
    }
    else {
      r_sockets.append(linked);
    }
  }
}
void IndexedNodeTree::find_connected_sockets_right(bNodeSocket *bsocket,
                                                   SmallVector<SocketWithNode> &r_sockets) const
{
  BLI_assert(bsocket->in_out == SOCK_OUT);
  auto to_sockets = m_direct_links.lookup_default(bsocket);
  for (SocketWithNode other : to_sockets) {
    if (this->is_reroute(other.node)) {
      this->find_connected_sockets_right((bNodeSocket *)other.node->outputs.first, r_sockets);
    }
    else {
      r_sockets.append(other);
    }
  }
}

bool IndexedNodeTree::is_reroute(bNode *bnode) const
{
  return STREQ(bnode->idname, "NodeReroute");
}

bool IndexedNodeTree::is_frame(bNode *bnode) const
{
  return STREQ(bnode->idname, "NodeFrame");
}

/* Queries
 *******************************************************/

ArrayRef<bNode *> IndexedNodeTree::nodes_with_idname(StringRef idname) const
{
  return m_nodes_by_idname.lookup_default(idname.to_std_string());
}

ArrayRef<SocketWithNode> IndexedNodeTree::linked(bNodeSocket *bsocket) const
{
  return m_links.lookup_default(bsocket);
}

ArrayRef<SingleOriginLink> IndexedNodeTree::single_origin_links() const
{
  return m_single_origin_links;
}

}  // namespace BKE
