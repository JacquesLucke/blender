#pragma once

#include "FN_core.hpp"
#include "BLI_optional.hpp"
#include "BKE_node_tree.hpp"

namespace FN {
namespace DataFlowNodes {

using BKE::IndexedNodeTree;

class GeneratedGraph {
 private:
  SharedDataFlowGraph m_graph;
  SmallMap<bNodeSocket *, DFGraphSocket> m_mapping;

 public:
  GeneratedGraph(SharedDataFlowGraph graph, SmallMap<bNodeSocket *, DFGraphSocket> mapping)
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

Optional<GeneratedGraph> generate_graph(IndexedNodeTree &indexed_btree);

Optional<FunctionGraph> generate_function_graph(IndexedNodeTree &indexed_btree);

}  // namespace DataFlowNodes
}  // namespace FN
