#pragma once

#include "DNA_node_types.h"

#include "BLI_string_ref.hpp"
#include "BLI_array_ref.hpp"
#include "BLI_small_map.hpp"
#include "BLI_small_vector.hpp"
#include "BLI_listbase_wrapper.hpp"
#include "BLI_multimap.hpp"

#include "RNA_access.h"

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

struct SocketWithNode {
  bNodeSocket *socket;
  bNode *node;
};

struct SingleOriginLink {
  bNodeSocket *from;
  bNodeSocket *to;
  bNodeLink *source_link;
};

/**
 * The DNA structure of a node tree is difficult to parse, since it does not support e.g. the
 * following queries efficiently:
 *   - Which nodes have a specific type?
 *   - Which node corresponds to a socket?
 *   - Which other sockets are connected to a socket (with and without reroutes)?
 *
 * This data structure does some preprocessing to make these queries more efficient. It is only
 * valid as long as the underlying node tree is not modified.
 */
class IndexedNodeTree {
 public:
  IndexedNodeTree(bNodeTree *btree);

  bNodeTree *btree() const
  {
    return m_btree;
  }

  ID *btree_id() const
  {
    return &m_btree->id;
  }

  PointerRNA get_rna(bNode *bnode) const
  {
    PointerRNA rna;
    RNA_pointer_create(this->btree_id(), &RNA_Node, bnode, &rna);
    return rna;
  }

  /**
   * Get all nodes that are in the btree->nodes list.
   */
  ArrayRef<bNode *> original_nodes() const
  {
    return m_original_nodes;
  }

  /**
   * Get all links that are in the btree->links list.
   */
  ArrayRef<bNodeLink *> original_links() const
  {
    return m_original_links;
  }

  /**
   * Get all nodes that are not reroutes or frames.
   */
  ArrayRef<bNode *> actual_nodes() const
  {
    return m_actual_nodes;
  }

  bNode *node_of_socket(bNodeSocket *bsocket) const
  {
    return m_node_by_socket.lookup(bsocket);
  }

  ArrayRef<SingleOriginLink> single_origin_links() const;
  ArrayRef<bNode *> nodes_with_idname(StringRef idname) const;
  ArrayRef<SocketWithNode> linked(bNodeSocket *bsocket) const;

 private:
  bool is_reroute(bNode *bnode) const;
  bool is_frame(bNode *bnode) const;

  void find_connected_sockets_left(bNodeSocket *bsocket,
                                   SmallVector<SocketWithNode> &r_sockets) const;
  void find_connected_sockets_right(bNodeSocket *bsocket,
                                    SmallVector<SocketWithNode> &r_sockets) const;

  bNodeTree *m_btree;
  SmallVector<bNode *> m_original_nodes;
  SmallVector<bNodeLink *> m_original_links;
  SmallVector<bNode *> m_actual_nodes;
  SmallMap<bNodeSocket *, bNode *> m_node_by_socket;
  SmallMultiMap<bNodeSocket *, SocketWithNode> m_direct_links;
  SmallMultiMap<bNodeSocket *, SocketWithNode> m_links;
  SmallMultiMap<std::string, bNode *> m_nodes_by_idname;
  SmallVector<SingleOriginLink> m_single_origin_links;
};

}  // namespace BKE
