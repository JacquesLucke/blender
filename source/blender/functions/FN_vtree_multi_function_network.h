#ifndef __FN_VTREE_MULTI_FUNCTION_NETWORK_H__
#define __FN_VTREE_MULTI_FUNCTION_NETWORK_H__

#include "BKE_virtual_node_tree.h"

#include "BLI_multi_map.h"

#include "FN_multi_function_network.h"

namespace FN {

using BKE::VInputSocket;
using BKE::VirtualNodeTree;
using BKE::VNode;
using BKE::VOutputSocket;
using BKE::VSocket;
using BLI::MultiMap;

#define VTreeMFSocketMap_UNMAPPED UINT_MAX
#define VTreeMFSocketMap_MULTIMAPPED (UINT_MAX - 1)

class VTreeMFSocketMap {
 private:
  /* An input vsocket can be mapped to multiple sockets.
   * An output vsocket can be mapped to at most one socket.
   */
  const VirtualNodeTree *m_vtree;
  const MFNetwork *m_network;
  Array<uint> m_single_socket_by_vsocket;
  MultiMap<uint, uint> m_multiple_inputs_by_vsocket;
  Array<uint> m_vsocket_by_socket;

 public:
  VTreeMFSocketMap(const VirtualNodeTree &vtree,
                   const MFNetwork &network,
                   Array<uint> single_socket_by_vsocket,
                   MultiMap<uint, uint> multiple_inputs_by_vsocket,
                   Array<uint> vsocket_by_socket)
      : m_vtree(&vtree),
        m_network(&network),
        m_single_socket_by_vsocket(std::move(single_socket_by_vsocket)),
        m_multiple_inputs_by_vsocket(std::move(multiple_inputs_by_vsocket)),
        m_vsocket_by_socket(std::move(vsocket_by_socket))
  {
  }

  bool is_mapped(const VSocket &vsocket) const
  {
    return m_single_socket_by_vsocket[vsocket.id()] < VTreeMFSocketMap_MULTIMAPPED;
  }

  bool is_mapped(const MFSocket &socket) const
  {
    return m_vsocket_by_socket[socket.id()] != VTreeMFSocketMap_UNMAPPED;
  }

  const MFInputSocket &lookup_singly_mapped_input_socket(const VInputSocket &vsocket) const
  {
    BLI_assert(this->lookup_socket(vsocket).size() == 1);
    uint mapped_id = m_single_socket_by_vsocket[vsocket.id()];
    return m_network->socket_by_id(mapped_id).as_input();
  }

  Vector<const MFInputSocket *> lookup_socket(const VInputSocket &vsocket) const
  {
    uint id = vsocket.id();
    uint mapped_value = m_single_socket_by_vsocket[id];
    switch (mapped_value) {
      case VTreeMFSocketMap_UNMAPPED: {
        return {};
      }
      case VTreeMFSocketMap_MULTIMAPPED: {
        Vector<const MFInputSocket *> sockets;
        for (uint mapped_id : m_multiple_inputs_by_vsocket.lookup(id)) {
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

  const MFOutputSocket &lookup_socket(const VOutputSocket &vsocket) const
  {
    uint mapped_id = m_single_socket_by_vsocket[vsocket.id()];
    return m_network->socket_by_id(mapped_id).as_output();
  }

  const VInputSocket &lookup_vsocket(const MFInputSocket &socket) const
  {
    uint mapped_id = m_vsocket_by_socket[socket.id()];
    return m_vtree->socket_by_id(mapped_id).as_input();
  }

  const VOutputSocket &lookup_vsocket(const MFOutputSocket &socket) const
  {
    uint mapped_id = m_vsocket_by_socket[socket.id()];
    return m_vtree->socket_by_id(mapped_id).as_output();
  }
};

class VTreeMFNetwork {
 private:
  const VirtualNodeTree &m_vtree;
  std::unique_ptr<MFNetwork> m_network;
  VTreeMFSocketMap m_socket_map;

 public:
  VTreeMFNetwork(const VirtualNodeTree &vtree,
                 std::unique_ptr<MFNetwork> network,
                 VTreeMFSocketMap socket_map)
      : m_vtree(vtree), m_network(std::move(network)), m_socket_map(std::move(socket_map))
  {
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
    return m_socket_map.is_mapped(vsocket);
  }

  bool is_mapped(const MFSocket &socket) const
  {
    return m_socket_map.is_mapped(socket);
  }

  const MFInputSocket &lookup_dummy_socket(const VInputSocket &vsocket) const
  {
    const MFInputSocket &socket = m_socket_map.lookup_singly_mapped_input_socket(vsocket);
    BLI_assert(socket.node().is_dummy());
    return socket;
  }

  const MFOutputSocket &lookup_socket(const VOutputSocket &vsocket) const
  {
    return m_socket_map.lookup_socket(vsocket);
  }

  const VInputSocket &lookup_vsocket(const MFInputSocket &socket) const
  {
    return m_socket_map.lookup_vsocket(socket);
  }

  const VOutputSocket &lookup_vsocket(const MFOutputSocket &socket) const
  {
    return m_socket_map.lookup_vsocket(socket);
  }

  void lookup_dummy_sockets(ArrayRef<const VOutputSocket *> vsockets,
                            MutableArrayRef<const MFOutputSocket *> r_result) const
  {
    BLI_assert(vsockets.size() == r_result.size());
    for (uint i = 0; i < vsockets.size(); i++) {
      r_result[i] = &this->lookup_socket(*vsockets[i]);
    }
  }

  void lookup_dummy_sockets(ArrayRef<const VInputSocket *> vsockets,
                            MutableArrayRef<const MFInputSocket *> r_result) const
  {
    BLI_assert(vsockets.size() == r_result.size());
    for (uint i = 0; i < vsockets.size(); i++) {
      r_result[i] = &this->lookup_dummy_socket(*vsockets[i]);
    }
  }
};

}  // namespace FN

#endif /* __FN_VTREE_MULTI_FUNCTION_NETWORK_H__ */
