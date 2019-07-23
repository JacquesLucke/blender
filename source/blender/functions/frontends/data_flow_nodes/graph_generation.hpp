#pragma once

#include "FN_core.hpp"
#include "BLI_optional.hpp"
#include "BKE_node_tree.hpp"

#include "builder.hpp"

namespace FN {
namespace DataFlowNodes {

using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;

class UnlinkedInputsHandler {
 public:
  virtual void insert(BTreeGraphBuilder &builder,
                      ArrayRef<VirtualSocket *> unlinked_inputs,
                      Vector<DFGB_Socket> &r_inserted_data_origins) = 0;
};

class VTreeDataGraph {
 private:
  SharedDataFlowGraph m_graph;
  Map<VirtualSocket *, DFGraphSocket> m_mapping;

 public:
  VTreeDataGraph(SharedDataFlowGraph graph, Map<VirtualSocket *, DFGraphSocket> mapping)
      : m_graph(std::move(graph)), m_mapping(std::move(mapping))
  {
  }

  SharedDataFlowGraph &graph()
  {
    return m_graph;
  }

  DFGraphSocket lookup_socket(VirtualSocket *vsocket)
  {
    return m_mapping.lookup(vsocket);
  }

  bool uses_socket(VirtualSocket *vsocket)
  {
    return m_mapping.contains(vsocket);
  }
};

Optional<VTreeDataGraph> generate_graph(VirtualNodeTree &vtree);

}  // namespace DataFlowNodes
}  // namespace FN
