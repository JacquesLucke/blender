#pragma once

#include "FN_core.hpp"
#include "util_wrappers.hpp"

namespace FN {
namespace DataFlowNodes {

class BTreeLookup {
 public:
  BTreeLookup(bNodeTree *btree);

  struct DataLink {
    bNodeSocket *from;
    bNodeSocket *to;
    bNodeLink *source_link;
  };

  const SmallVector<DataLink> data_links()
  {
    return m_data_links;
  }

 private:
  bool is_reroute(bNode *bnode);
  bNodeSocket *try_find_data_origin(bNodeSocket *bsocket);

  SmallMap<bNodeSocket *, bNode *> m_node_by_socket;
  SmallMap<bNodeSocket *, bNodeSocket *> m_direct_origin;
  SmallVector<DataLink> m_data_links;
};

}  // namespace DataFlowNodes
}  // namespace FN
