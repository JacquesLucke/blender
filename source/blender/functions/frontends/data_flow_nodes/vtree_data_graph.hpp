#pragma once

#include "FN_core.hpp"
#include "BKE_node_tree.hpp"
#include "BLI_value_or_error.h"
#include "BLI_array_cxx.h"

namespace FN {
namespace DataFlowNodes {

using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;
using BLI::Array;
using BLI::ValueOrError;

class VTreeDataGraph {
 private:
  VirtualNodeTree &m_vtree;
  SharedDataGraph m_graph;
  Array<DataSocket> m_socket_map;

 public:
  VTreeDataGraph(VirtualNodeTree &vtree, SharedDataGraph graph, Array<DataSocket> mapping);

  VirtualNodeTree &vtree()
  {
    return m_vtree;
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

  Type *lookup_type(VirtualSocket *vsocket)
  {
    DataSocket socket = this->lookup_socket(vsocket);
    return m_graph->type_of_socket(socket);
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
  Vector<VirtualSocket *> m_vsocket_inputs;

 public:
  static const uint FUNCTION_BODY_ID = 4;

  VNodePlaceholderBody(VirtualNode *vnode, Vector<VirtualSocket *> vsocket_inputs)
      : m_vnode(vnode), m_vsocket_inputs(std::move(vsocket_inputs))
  {
  }

  VirtualNode *vnode()
  {
    return m_vnode;
  }

  ArrayRef<VirtualSocket *> inputs()
  {
    return m_vsocket_inputs;
  }
};

}  // namespace DataFlowNodes
}  // namespace FN
