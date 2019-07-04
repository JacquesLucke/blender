#pragma once

#include "DNA_node_types.h"

#include "BLI_string_ref.hpp"
#include "BLI_array_ref.hpp"
#include "BLI_small_map.hpp"
#include "BLI_small_vector.hpp"
#include "BLI_listbase_wrapper.hpp"
#include "BLI_multimap.hpp"

namespace BKE {

using BLI::ArrayRef;
using BLI::ListBaseWrapper;
using BLI::SmallMap;
using BLI::SmallMultiMap;
using BLI::SmallVector;
using BLI::StringRef;

using bNodeList = ListBaseWrapper<struct bNode *, true>;
using bLinkList = ListBaseWrapper<struct bNodeLink *, true>;
using bSocketList = ListBaseWrapper<struct bNodeSocket *, true>;

/**
 * The DNA structure of a node tree is difficult to parse, since it does not support e.g. the
 * following queries efficiently:
 *   - Which nodes have a specific type?
 *   - Which node corresponds to a socket?
 *   - Which other sockets are connected to a socket (with and without reroutes)?
 *
 * This data structure does some preprocessing to make these queries more efficient.
 */
class IndexedNodeTree {
 public:
  IndexedNodeTree(bNodeTree *btree);

  struct SingleOriginLink {
    bNodeSocket *from;
    bNodeSocket *to;
    bNodeLink *source_link;
  };

  const ArrayRef<SingleOriginLink> single_origin_links() const
  {
    return m_single_origin_links;
  }

  SmallVector<bNode *> nodes_with_idname(StringRef idname) const;
  SmallVector<bNode *> nodes_connected_to_socket(bNodeSocket *bsocket) const;

 private:
  bool is_reroute(bNode *bnode) const;

  void find_connected_sockets_left(bNodeSocket *bsocket,
                                   SmallVector<bNodeSocket *> &r_sockets) const;
  void find_connected_sockets_right(bNodeSocket *bsocket,
                                    SmallVector<bNodeSocket *> &r_sockets) const;

  SmallVector<bNode *> m_nodes;
  SmallVector<bNodeLink *> m_links;
  SmallMap<bNodeSocket *, bNode *> m_node_by_socket;
  SmallMultiMap<bNodeSocket *, bNodeSocket *> m_direct_links;
  SmallMultiMap<bNodeSocket *, bNodeSocket *> m_links_without_reroutes;
  SmallVector<SingleOriginLink> m_single_origin_links;
};

}  // namespace BKE
