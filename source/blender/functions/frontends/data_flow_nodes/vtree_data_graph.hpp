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

  Vector<DataSocket> lookup_sockets(ArrayRef<VirtualSocket *> vsockets)
  {
    Vector<DataSocket> sockets;
    sockets.reserve(vsockets.size());
    for (VirtualSocket *vsocket : vsockets) {
      sockets.append(this->lookup_socket(vsocket));
    }
    return sockets;
  }

  DataSocket lookup_socket(VirtualSocket *vsocket)
  {
    return m_socket_map[vsocket->id()];
  }

  bool uses_socket(VirtualSocket *vsocket)
  {
    return !m_socket_map[vsocket->id()].is_none();
  }

  Vector<VirtualSocket *> find_placeholder_dependencies(ArrayRef<VirtualSocket *> vsockets);
  Vector<VirtualSocket *> find_placeholder_dependencies(ArrayRef<DataSocket> sockets);

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
