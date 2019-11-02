#pragma once

#include "FN_core.hpp"
#include "BKE_virtual_node_tree.h"
#include "BLI_array_cxx.h"

namespace FN {
namespace DataFlowNodes {

using BKE::VirtualNodeTree;
using BKE::VNode;
using BKE::VSocket;
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

  DataSocket *lookup_socket_ptr(const VSocket &vsocket)
  {
    DataSocket *socket = &m_socket_map[vsocket.id()];
    if (socket->is_none()) {
      return nullptr;
    }
    return socket;
  }

  Vector<DataSocket> lookup_sockets(ArrayRef<const VSocket *> vsockets)
  {
    Vector<DataSocket> sockets;
    sockets.reserve(vsockets.size());
    for (const VSocket *vsocket : vsockets) {
      sockets.append(this->lookup_socket(*vsocket));
    }
    return sockets;
  }

  DataSocket lookup_socket(const VSocket &vsocket)
  {
    return m_socket_map[vsocket.id()];
  }

  Type *lookup_type(const VSocket &vsocket)
  {
    DataSocket socket = this->lookup_socket(vsocket);
    return m_graph->type_of_socket(socket);
  }

  bool uses_socket(const VSocket &vsocket)
  {
    return !m_socket_map[vsocket.id()].is_none();
  }

  Vector<const VSocket *> find_placeholder_dependencies(ArrayRef<const VSocket *> vsockets);
  Vector<const VSocket *> find_placeholder_dependencies(ArrayRef<DataSocket> sockets);

 private:
  const VSocket &find_data_output(const VNode &vnode, uint index);
};

class VNodePlaceholderBody : public FunctionBody {
 private:
  const VNode *m_vnode;
  Vector<const VSocket *> m_vsocket_inputs;

 public:
  static const uint FUNCTION_BODY_ID = 4;
  using FunctionBodyType = VNodePlaceholderBody;

  VNodePlaceholderBody(const VNode &vnode, Vector<const VSocket *> vsocket_inputs)
      : m_vnode(&vnode), m_vsocket_inputs(std::move(vsocket_inputs))
  {
  }

  const VNode &vnode()
  {
    return *m_vnode;
  }

  ArrayRef<const VSocket *> inputs()
  {
    return m_vsocket_inputs;
  }
};

}  // namespace DataFlowNodes
}  // namespace FN
