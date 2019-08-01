#pragma once

#include "FN_core.hpp"
#include "BKE_node_tree.hpp"
#include "BLI_value_or_error.hpp"

namespace FN {
namespace DataFlowNodes {

using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;
using BLI::ValueOrError;

class VTreeDataGraph {
 private:
  SharedDataGraph m_graph;
  Vector<DataSocket> m_socket_map;

 public:
  VTreeDataGraph(SharedDataGraph graph, Vector<DataSocket> mapping)
      : m_graph(std::move(graph)), m_socket_map(std::move(mapping))
  {
  }

  SharedDataGraph &graph()
  {
    return m_graph;
  }

  DataSocket *lookup_socket_ptr(VirtualSocket *vsocket)
  {
    DataSocket *socket = &m_socket_map[vsocket->id()];
    if (socket->is_none()) {
      return nullptr;
    }
    return socket;
  }

  DataSocket lookup_socket(VirtualSocket *vsocket)
  {
    return m_socket_map[vsocket->id()];
  }

  bool uses_socket(VirtualSocket *vsocket)
  {
    return !m_socket_map[vsocket->id()].is_none();
  }

  struct PlaceholderDependencies {
    Vector<VirtualSocket *> vsockets;
    Vector<DataSocket> sockets;

    uint size() const
    {
      BLI_assert(this->vsockets.size() == this->sockets.size());
      return this->vsockets.size();
    }
  };

  PlaceholderDependencies find_placeholder_dependencies(ArrayRef<VirtualSocket *> vsockets);
  PlaceholderDependencies find_placeholder_dependencies(ArrayRef<DataSocket> sockets);

 private:
  VirtualSocket *find_data_output(VirtualNode *vnode, uint index);
};

class VNodePlaceholderBody : public FunctionBody {
 private:
  VirtualNode *m_vnode;

 public:
  static const uint FUNCTION_BODY_ID = 4;

  VNodePlaceholderBody(VirtualNode *vnode) : m_vnode(vnode)
  {
  }

  VirtualNode *vnode()
  {
    return m_vnode;
  }
};

}  // namespace DataFlowNodes
}  // namespace FN
