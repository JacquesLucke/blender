#include "btree_lookup.hpp"

#include "DNA_node_types.h"

namespace FN {
namespace DataFlowNodes {

BTreeLookup::BTreeLookup(bNodeTree *btree)
{
  for (bNode *bnode : bNodeList(&btree->nodes)) {
    for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
      m_node_by_socket.add(bsocket, bnode);
    }
    for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
      m_node_by_socket.add(bsocket, bnode);
    }
  }

  for (bNodeLink *blink : bLinkList(&btree->links)) {
    BLI_assert(!m_direct_origin.contains(blink->tosock));
    m_direct_origin.add(blink->tosock, blink->fromsock);
  }

  for (bNodeLink *blink : bLinkList(&btree->links)) {
    bNodeSocket *target = blink->tosock;
    bNode *target_node = blink->tonode;
    if (this->is_reroute(target_node)) {
      continue;
    }
    bNodeSocket *origin = this->try_find_data_origin(target);
    if (origin != nullptr) {
      m_data_links.append(DataLink{origin, target, blink});
    }
  }
}

bNodeSocket *BTreeLookup::try_find_data_origin(bNodeSocket *bsocket)
{
  BLI_assert(bsocket->in_out == SOCK_IN);
  if (m_direct_origin.contains(bsocket)) {
    bNodeSocket *origin = m_direct_origin.lookup(bsocket);
    bNode *origin_node = m_node_by_socket.lookup(origin);
    if (this->is_reroute(origin_node)) {
      return this->try_find_data_origin((bNodeSocket *)origin_node->inputs.first);
    }
    else {
      return origin;
    }
  }
  else {
    return nullptr;
  }
}

bool BTreeLookup::is_reroute(bNode *bnode)
{
  return STREQ(bnode->idname, "NodeReroute");
}

}  // namespace DataFlowNodes
}  // namespace FN
