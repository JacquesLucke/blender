#include "BKE_node_tree.hpp"

namespace BKE {

BNodeTreeLookup::BNodeTreeLookup(bNodeTree *btree) : m_nodes(btree->nodes, true)
{
  for (bNode *bnode : m_nodes) {
    for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
      m_node_by_socket.add(bsocket, bnode);
    }
    for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
      m_node_by_socket.add(bsocket, bnode);
    }
  }

  for (bNodeLink *blink : bLinkList(&btree->links)) {
    m_direct_origins.add(blink->tosock, blink->fromsock);
  }

  for (bNodeLink *blink : bLinkList(&btree->links)) {
    bNodeSocket *target = blink->tosock;
    bNode *target_node = blink->tonode;
    if (this->is_reroute(target_node)) {
      continue;
    }
    bNodeSocket *origin = this->try_find_single_origin(target);
    if (origin != nullptr) {
      m_single_origin_links.append(SingleOriginLink{origin, target, blink});
    }
  }
}

bNodeSocket *BNodeTreeLookup::try_find_single_origin(bNodeSocket *bsocket)
{
  BLI_assert(bsocket->in_out == SOCK_IN);
  if (m_direct_origins.values_for_key(bsocket) == 1) {
    bNodeSocket *origin = m_direct_origins.lookup(bsocket)[0];
    bNode *origin_node = m_node_by_socket.lookup(origin);
    if (this->is_reroute(origin_node)) {
      return this->try_find_single_origin((bNodeSocket *)origin_node->inputs.first);
    }
    else {
      return origin;
    }
  }
  else {
    return nullptr;
  }
}

bool BNodeTreeLookup::is_reroute(bNode *bnode)
{
  return STREQ(bnode->idname, "NodeReroute");
}

SmallVector<bNode *> BNodeTreeLookup::nodes_with_idname(StringRef idname)
{
  SmallVector<bNode *> result;
  for (bNode *bnode : m_nodes) {
    if (bnode->idname == idname) {
      result.append(bnode);
    }
  }
  return result;
}

}  // namespace BKE
