#pragma once

#include "FN_core.hpp"
#include "BKE_node_tree.hpp"

namespace FN {
namespace DataFlowNodes {

using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;
using BLI::ValueOrError;

class VTreeDataGraph {
 private:
  SharedDataFlowGraph m_graph;
  Map<VirtualSocket *, DFGraphSocket> m_socket_map;

 public:
  VTreeDataGraph(SharedDataFlowGraph graph, Map<VirtualSocket *, DFGraphSocket> mapping)
      : m_graph(std::move(graph)), m_socket_map(std::move(mapping))
  {
  }

  SharedDataFlowGraph &graph()
  {
    return m_graph;
  }

  DFGraphSocket *lookup_socket_ptr(VirtualSocket *vsocket)
  {
    return m_socket_map.lookup_ptr(vsocket);
  }

  DFGraphSocket lookup_socket(VirtualSocket *vsocket)
  {
    return m_socket_map.lookup(vsocket);
  }

  bool uses_socket(VirtualSocket *vsocket)
  {
    return m_socket_map.contains(vsocket);
  }

  struct PlaceholderDependencies {
    Vector<VirtualSocket *> vsockets;
    Vector<DFGraphSocket> sockets;

    uint size() const
    {
      BLI_assert(this->vsockets.size() == this->sockets.size());
      return this->vsockets.size();
    }
  };

  PlaceholderDependencies find_placeholder_dependencies(ArrayRef<VirtualSocket *> vsockets);
  PlaceholderDependencies find_placeholder_dependencies(ArrayRef<DFGraphSocket> sockets);

 private:
  VirtualSocket *find_data_output(VirtualNode *vnode, uint index);
};

}  // namespace DataFlowNodes
}  // namespace FN
