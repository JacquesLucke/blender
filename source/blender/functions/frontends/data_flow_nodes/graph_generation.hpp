#pragma once

#include "FN_core.hpp"
#include "BLI_optional.hpp"
#include "BKE_node_tree.hpp"

#include "builder.hpp"

namespace FN {
namespace DataFlowNodes {

using BKE::IndexedNodeTree;
using BKE::SocketWithNode;

class UnlinkedInputsHandler {
 public:
  virtual void insert(BTreeGraphBuilder &builder,
                      ArrayRef<bNodeSocket *> unlinked_inputs,
                      DFGB_SocketVector &r_inserted_data_origins) = 0;
};

class BTreeDataGraph {
 private:
  SharedDataFlowGraph m_graph;
  SmallMap<bNodeSocket *, DFGraphSocket> m_mapping;

 public:
  BTreeDataGraph(SharedDataFlowGraph graph, SmallMap<bNodeSocket *, DFGraphSocket> mapping)
      : m_graph(std::move(graph)), m_mapping(std::move(mapping))
  {
  }

  SharedDataFlowGraph &graph()
  {
    return m_graph;
  }

  DFGraphSocket lookup_socket(bNodeSocket *bsocket)
  {
    return m_mapping.lookup(bsocket);
  }
};

Optional<BTreeDataGraph> generate_graph(IndexedNodeTree &indexed_btree);

Optional<FunctionGraph> generate_function_graph(IndexedNodeTree &indexed_btree);

}  // namespace DataFlowNodes
}  // namespace FN
