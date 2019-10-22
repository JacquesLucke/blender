#pragma once

#include "FN_core.hpp"
#include "BKE_virtual_node_tree_cxx.h"
#include "BLI_array_cxx.h"

namespace FN {
namespace DataFlowNodes {

using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;
using BLI::Array;

class VTreeDataGraph {
 private:
  VirtualNodeTree &m_vtree;
  std::unique_ptr<DataGraph> m_graph;
  Array<DataSocket> m_socket_map;

 public:
  VTreeDataGraph(VirtualNodeTree &vtree,
                 std::unique_ptr<DataGraph> graph,
                 Array<DataSocket> mapping);

  VirtualNodeTree &vtree()
  {
    return m_vtree;
  }

  DataGraph &graph()
  {
    return *m_graph;
  }

  DataSocket *lookup_socket_ptr(const VirtualSocket &vsocket)
  {
    DataSocket *socket = &m_socket_map[vsocket.id()];
    if (socket->is_none()) {
      return nullptr;
    }
    return socket;
  }

  Vector<DataSocket> lookup_sockets(ArrayRef<const VirtualSocket *> vsockets)
  {
    Vector<DataSocket> sockets;
    sockets.reserve(vsockets.size());
    for (const VirtualSocket *vsocket : vsockets) {
      sockets.append(this->lookup_socket(*vsocket));
    }
    return sockets;
  }

  DataSocket lookup_socket(const VirtualSocket &vsocket)
  {
    return m_socket_map[vsocket.id()];
  }

  Type *lookup_type(const VirtualSocket &vsocket)
  {
    DataSocket socket = this->lookup_socket(vsocket);
    return m_graph->type_of_socket(socket);
  }

  bool uses_socket(const VirtualSocket &vsocket)
  {
    return !m_socket_map[vsocket.id()].is_none();
  }

  Vector<const VirtualSocket *> find_placeholder_dependencies(
      ArrayRef<const VirtualSocket *> vsockets);
  Vector<const VirtualSocket *> find_placeholder_dependencies(ArrayRef<DataSocket> sockets);

 private:
  const VirtualSocket &find_data_output(const VirtualNode &vnode, uint index);
};

class VNodePlaceholderBody : public FunctionBody {
 private:
  const VirtualNode *m_vnode;
  Vector<const VirtualSocket *> m_vsocket_inputs;

 public:
  static const uint FUNCTION_BODY_ID = 4;
  using FunctionBodyType = VNodePlaceholderBody;

  VNodePlaceholderBody(const VirtualNode &vnode, Vector<const VirtualSocket *> vsocket_inputs)
      : m_vnode(&vnode), m_vsocket_inputs(std::move(vsocket_inputs))
  {
  }

  const VirtualNode &vnode()
  {
    return *m_vnode;
  }

  ArrayRef<const VirtualSocket *> inputs()
  {
    return m_vsocket_inputs;
  }
};

}  // namespace DataFlowNodes
}  // namespace FN
