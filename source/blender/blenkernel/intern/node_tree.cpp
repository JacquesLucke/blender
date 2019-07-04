#include "BKE_node_tree.hpp"
#include "BLI_timeit.hpp"

namespace BKE {

IndexedNodeTree::IndexedNodeTree(bNodeTree *btree)
    : m_nodes(btree->nodes, true), m_links(btree->links, true)
{
  for (bNode *bnode : m_nodes) {
    for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
      m_node_by_socket.add(bsocket, bnode);
    }
    for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
      m_node_by_socket.add(bsocket, bnode);
    }
  }

  for (bNodeLink *blink : m_links) {
    m_direct_links.add(blink->tosock, blink->fromsock);
    m_direct_links.add(blink->fromsock, blink->tosock);
  }

  for (bNodeLink *blink : m_links) {
    if (!this->is_reroute(blink->fromnode) &&
        !m_links_without_reroutes.contains(blink->fromsock)) {
      SmallVector<bNodeSocket *> others;
      this->find_connected_sockets_right(blink->fromsock, others);
      m_links_without_reroutes.add_multiple_new(blink->fromsock, others);
    }
    if (!this->is_reroute(blink->tonode) && !m_links_without_reroutes.contains(blink->tosock)) {
      SmallVector<bNodeSocket *> others;
      this->find_connected_sockets_left(blink->tosock, others);
      m_links_without_reroutes.add_multiple_new(blink->tosock, others);
      if (others.size() == 1) {
        m_single_origin_links.append(SingleOriginLink{others[0], blink->tosock, blink});
      }
    }
  }
}

void IndexedNodeTree::find_connected_sockets_left(bNodeSocket *bsocket,
                                                  SmallVector<bNodeSocket *> &r_sockets) const
{
  BLI_assert(bsocket->in_out == SOCK_IN);
  for (bNodeSocket *other : m_direct_links.lookup_default(bsocket)) {
    bNode *other_node = m_node_by_socket.lookup(other);
    if (this->is_reroute(other_node)) {
      this->find_connected_sockets_left((bNodeSocket *)other_node->inputs.first, r_sockets);
    }
    else {
      r_sockets.append(other);
    }
  }
}
void IndexedNodeTree::find_connected_sockets_right(bNodeSocket *bsocket,
                                                   SmallVector<bNodeSocket *> &r_sockets) const
{
  BLI_assert(bsocket->in_out == SOCK_OUT);
  for (bNodeSocket *other : m_direct_links.lookup_default(bsocket)) {
    bNode *other_node = m_node_by_socket.lookup(other);
    if (this->is_reroute(other_node)) {
      this->find_connected_sockets_right((bNodeSocket *)other_node->outputs.first, r_sockets);
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

SmallVector<bNode *> IndexedNodeTree::nodes_with_idname(StringRef idname) const
{
  SmallVector<bNode *> result;
  for (bNode *bnode : m_nodes) {
    if (bnode->idname == idname) {
      result.append(bnode);
    }
  }
  return result;
}

SmallVector<bNode *> IndexedNodeTree::nodes_connected_to_socket(bNodeSocket *bsocket) const
{
  SmallVector<bNode *> result;
  for (bNodeSocket *other : m_links_without_reroutes.lookup_default(bsocket)) {
    bNode *bnode = m_node_by_socket.lookup(other);
    result.append(bnode);
  }
  return result;
}

}  // namespace BKE
