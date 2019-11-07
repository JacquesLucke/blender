#ifndef __FN_VTREE_MULTI_FUNCTION_NETWORK_H__
#define __FN_VTREE_MULTI_FUNCTION_NETWORK_H__

#include "BKE_virtual_node_tree.h"

#include "FN_multi_function_network.h"

namespace FN {

using BKE::VInputSocket;
using BKE::VirtualNodeTree;
using BKE::VNode;
using BKE::VOutputSocket;
using BKE::VSocket;

class VTreeMFNetwork {
 private:
  const VirtualNodeTree &m_vtree;
  std::unique_ptr<MFNetwork> m_network;
  Array<const MFSocket *> m_socket_by_vsocket;
  Array<const VSocket *> m_vsocket_by_socket;

 public:
  VTreeMFNetwork(const VirtualNodeTree &vtree,
                 std::unique_ptr<MFNetwork> network,
                 Array<const MFSocket *> socket_map)
      : m_vtree(vtree), m_network(std::move(network)), m_socket_by_vsocket(std::move(socket_map))
  {
    m_vsocket_by_socket = Array<const VSocket *>(m_socket_by_vsocket.size());
    for (uint vsocket_id = 0; vsocket_id < m_socket_by_vsocket.size(); vsocket_id++) {
      const MFSocket *socket = m_socket_by_vsocket[vsocket_id];
      if (socket != nullptr) {
        const VSocket &vsocket = m_vtree.socket_by_id(vsocket_id);
        m_vsocket_by_socket[socket->id()] = &vsocket;
      }
    }
  }

  const VirtualNodeTree &vtree() const
  {
    return m_vtree;
  }

  const MFNetwork &network() const
  {
    return *m_network;
  }

  bool is_mapped(const VSocket &vsocket) const
  {
    return m_socket_by_vsocket[vsocket.id()] != nullptr;
  }

  bool is_mapped(const MFSocket &socket) const
  {
    return m_vsocket_by_socket[socket.id()] != nullptr;
  }

  const MFInputSocket &lookup_socket(const VInputSocket &vsocket) const
  {
    return m_socket_by_vsocket[vsocket.id()]->as_input();
  }

  const MFOutputSocket &lookup_socket(const VOutputSocket &vsocket) const
  {
    return m_socket_by_vsocket[vsocket.id()]->as_output();
  }

  const VInputSocket &lookup_vsocket(const MFInputSocket &socket) const
  {
    return m_vsocket_by_socket[socket.id()]->as_input();
  }

  const VOutputSocket &lookup_vsocket(const MFOutputSocket &socket) const
  {
    return m_vsocket_by_socket[socket.id()]->as_output();
  }

  void lookup_sockets(ArrayRef<const VOutputSocket *> vsockets,
                      MutableArrayRef<const MFOutputSocket *> r_result) const
  {
    BLI_assert(vsockets.size() == r_result.size());
    for (uint i = 0; i < vsockets.size(); i++) {
      r_result[i] = &this->lookup_socket(*vsockets[i]);
    }
  }

  void lookup_sockets(ArrayRef<const VInputSocket *> vsockets,
                      MutableArrayRef<const MFInputSocket *> r_result) const
  {
    BLI_assert(vsockets.size() == r_result.size());
    for (uint i = 0; i < vsockets.size(); i++) {
      r_result[i] = &this->lookup_socket(*vsockets[i]);
    }
  }
};

}  // namespace FN

#endif /* __FN_VTREE_MULTI_FUNCTION_NETWORK_H__ */
