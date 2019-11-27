#ifndef __FN_VTREE_MULTI_FUNCTION_NETWORK_H__
#define __FN_VTREE_MULTI_FUNCTION_NETWORK_H__

#include "BKE_inlined_node_tree.h"

#include "BLI_multi_map.h"

#include "FN_multi_function_network.h"

namespace FN {

using BKE::InlinedNodeTree;
using BKE::XInputSocket;
using BKE::XNode;
using BKE::XOutputSocket;
using BKE::XSocket;
using BLI::MultiMap;

#define VTreeMFSocketMap_UNMAPPED UINT_MAX
#define VTreeMFSocketMap_MULTIMAPPED (UINT_MAX - 1)

class VTreeMFSocketMap {
 private:
  /* An input xsocket can be mapped to multiple sockets.
   * An output xsocket can be mapped to at most one socket.
   */
  const InlinedNodeTree *m_inlined_tree;
  const MFNetwork *m_network;
  Array<uint> m_single_socket_by_xsocket;
  MultiMap<uint, uint> m_multiple_inputs_by_xsocket;
  Array<uint> m_xsocket_by_socket;

 public:
  VTreeMFSocketMap(const InlinedNodeTree &inlined_tree,
                   const MFNetwork &network,
                   Array<uint> single_socket_by_xsocket,
                   MultiMap<uint, uint> multiple_inputs_by_xsocket,
                   Array<uint> xsocket_by_socket)
      : m_inlined_tree(&inlined_tree),
        m_network(&network),
        m_single_socket_by_xsocket(std::move(single_socket_by_xsocket)),
        m_multiple_inputs_by_xsocket(std::move(multiple_inputs_by_xsocket)),
        m_xsocket_by_socket(std::move(xsocket_by_socket))
  {
  }

  bool is_mapped(const XSocket &xsocket) const
  {
    return m_single_socket_by_xsocket[xsocket.id()] < VTreeMFSocketMap_MULTIMAPPED;
  }

  bool is_mapped(const MFSocket &socket) const
  {
    return m_xsocket_by_socket[socket.id()] != VTreeMFSocketMap_UNMAPPED;
  }

  const MFInputSocket &lookup_singly_mapped_input_socket(const XInputSocket &xsocket) const
  {
    BLI_assert(this->lookup_socket(xsocket).size() == 1);
    uint mapped_id = m_single_socket_by_xsocket[xsocket.id()];
    return m_network->socket_by_id(mapped_id).as_input();
  }

  Vector<const MFInputSocket *> lookup_socket(const XInputSocket &xsocket) const
  {
    uint id = xsocket.id();
    uint mapped_value = m_single_socket_by_xsocket[id];
    switch (mapped_value) {
      case VTreeMFSocketMap_UNMAPPED: {
        return {};
      }
      case VTreeMFSocketMap_MULTIMAPPED: {
        Vector<const MFInputSocket *> sockets;
        for (uint mapped_id : m_multiple_inputs_by_xsocket.lookup(id)) {
          sockets.append(&m_network->socket_by_id(mapped_id).as_input());
        }
        return sockets;
      }
      default: {
        uint mapped_id = mapped_value;
        const MFInputSocket &socket = m_network->socket_by_id(mapped_id).as_input();
        return {&socket};
      }
    }
  }

  const MFOutputSocket &lookup_socket(const XOutputSocket &xsocket) const
  {
    uint mapped_id = m_single_socket_by_xsocket[xsocket.id()];
    return m_network->socket_by_id(mapped_id).as_output();
  }

  const XInputSocket &lookup_xsocket(const MFInputSocket &socket) const
  {
    uint mapped_id = m_xsocket_by_socket[socket.id()];
    return m_inlined_tree->socket_by_id(mapped_id).as_input();
  }

  const XOutputSocket &lookup_xsocket(const MFOutputSocket &socket) const
  {
    uint mapped_id = m_xsocket_by_socket[socket.id()];
    return m_inlined_tree->socket_by_id(mapped_id).as_output();
  }
};

class VTreeMFNetwork {
 private:
  const InlinedNodeTree &m_inlined_tree;
  std::unique_ptr<MFNetwork> m_network;
  VTreeMFSocketMap m_socket_map;

 public:
  VTreeMFNetwork(const InlinedNodeTree &inlined_tree,
                 std::unique_ptr<MFNetwork> network,
                 VTreeMFSocketMap socket_map)
      : m_inlined_tree(inlined_tree),
        m_network(std::move(network)),
        m_socket_map(std::move(socket_map))
  {
  }

  const InlinedNodeTree &inlined_tree() const
  {
    return m_inlined_tree;
  }

  const MFNetwork &network() const
  {
    return *m_network;
  }

  bool is_mapped(const XSocket &xsocket) const
  {
    return m_socket_map.is_mapped(xsocket);
  }

  bool is_mapped(const MFSocket &socket) const
  {
    return m_socket_map.is_mapped(socket);
  }

  const MFInputSocket &lookup_dummy_socket(const XInputSocket &xsocket) const
  {
    const MFInputSocket &socket = m_socket_map.lookup_singly_mapped_input_socket(xsocket);
    BLI_assert(socket.node().is_dummy());
    return socket;
  }

  const MFOutputSocket &lookup_dummy_socket(const XOutputSocket &xsocket) const
  {
    const MFOutputSocket &socket = this->lookup_socket(xsocket);
    BLI_assert(socket.node().is_dummy());
    return socket;
  }

  const MFOutputSocket &lookup_socket(const XOutputSocket &xsocket) const
  {
    return m_socket_map.lookup_socket(xsocket);
  }

  const XInputSocket &lookup_xsocket(const MFInputSocket &socket) const
  {
    return m_socket_map.lookup_xsocket(socket);
  }

  const XOutputSocket &lookup_xsocket(const MFOutputSocket &socket) const
  {
    return m_socket_map.lookup_xsocket(socket);
  }

  void lookup_dummy_sockets(ArrayRef<const XOutputSocket *> xsockets,
                            MutableArrayRef<const MFOutputSocket *> r_result) const
  {
    BLI_assert(xsockets.size() == r_result.size());
    for (uint i : xsockets.index_iterator()) {
      r_result[i] = &this->lookup_socket(*xsockets[i]);
    }
  }

  void lookup_dummy_sockets(ArrayRef<const XInputSocket *> xsockets,
                            MutableArrayRef<const MFInputSocket *> r_result) const
  {
    BLI_assert(xsockets.size() == r_result.size());
    for (uint i : xsockets.index_iterator()) {
      r_result[i] = &this->lookup_dummy_socket(*xsockets[i]);
    }
  }
};

}  // namespace FN

#endif /* __FN_VTREE_MULTI_FUNCTION_NETWORK_H__ */
