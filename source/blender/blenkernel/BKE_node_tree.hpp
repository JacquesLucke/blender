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
using BLI::MultiMap;
using BLI::SmallMap;
using BLI::SmallVector;
using BLI::StringRef;

using bNodeList = ListBaseWrapper<struct bNode *, true>;
using bLinkList = ListBaseWrapper<struct bNodeLink *, true>;
using bSocketList = ListBaseWrapper<struct bNodeSocket *, true>;

class NodeTreeQuery {
 public:
  NodeTreeQuery(bNodeTree *btree);

  struct SingleOriginLink {
    bNodeSocket *from;
    bNodeSocket *to;
    bNodeLink *source_link;
  };

  const ArrayRef<SingleOriginLink> single_origin_links()
  {
    return m_single_origin_links;
  }

  SmallVector<bNode *> nodes_with_idname(StringRef idname);
  SmallVector<bNode *> nodes_connected_to_socket(bNodeSocket *bsocket);

 private:
  bool is_reroute(bNode *bnode);
  bNodeSocket *try_find_single_origin(bNodeSocket *bsocket);

  SmallVector<bNode *> m_nodes;
  SmallMap<bNodeSocket *, bNode *> m_node_by_socket;
  MultiMap<bNodeSocket *, bNodeSocket *> m_direct_links;
  SmallVector<SingleOriginLink> m_single_origin_links;
};

}  // namespace BKE
