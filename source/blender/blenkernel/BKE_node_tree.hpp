#pragma once

#include "DNA_node_types.h"

#include "BLI_string_ref.hpp"
#include "BLI_array_ref.hpp"
#include "BLI_small_map.hpp"
#include "BLI_small_vector.hpp"
#include "BLI_listbase_wrapper.hpp"

namespace BKE {

using BLI::ArrayRef;
using BLI::ListBaseWrapper;
using BLI::SmallMap;
using BLI::SmallVector;
using BLI::StringRef;

using bNodeList = ListBaseWrapper<struct bNode *, true>;
using bLinkList = ListBaseWrapper<struct bNodeLink *, true>;
using bSocketList = ListBaseWrapper<struct bNodeSocket *, true>;

class BNodeTreeLookup {
 public:
  BNodeTreeLookup(bNodeTree *btree);

  struct DataLink {
    bNodeSocket *from;
    bNodeSocket *to;
    bNodeLink *source_link;
  };

  const ArrayRef<DataLink> data_links()
  {
    return m_data_links;
  }

  SmallVector<bNode *> nodes_with_idname(StringRef idname);

 private:
  bool is_reroute(bNode *bnode);
  bNodeSocket *try_find_data_origin(bNodeSocket *bsocket);

  SmallVector<bNode *> m_nodes;
  SmallMap<bNodeSocket *, bNode *> m_node_by_socket;
  SmallMap<bNodeSocket *, bNodeSocket *> m_direct_origin;
  SmallVector<DataLink> m_data_links;
};

}  // namespace BKE
