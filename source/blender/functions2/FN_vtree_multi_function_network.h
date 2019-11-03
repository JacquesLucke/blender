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
  Array<const MFSocket *> m_socket_map;

 public:
  VTreeMFNetwork(const VirtualNodeTree &vtree,
                 std::unique_ptr<MFNetwork> network,
                 Array<const MFSocket *> socket_map)
      : m_vtree(vtree), m_network(std::move(network)), m_socket_map(std::move(socket_map))
  {
  }

  const VirtualNodeTree &vtree()
  {
    return m_vtree;
  }

  const MFNetwork &network()
  {
    return *m_network;
  }

  const MFInputSocket &lookup_socket(const VInputSocket &vsocket)
  {
    return m_socket_map[vsocket.id()]->as_input();
  }

  const MFOutputSocket &lookup_socket(const VOutputSocket &vsocket)
  {
    return m_socket_map[vsocket.id()]->as_output();
  }

  void lookup_sockets(ArrayRef<const VOutputSocket *> vsockets,
                      MutableArrayRef<const MFOutputSocket *> r_result)
  {
    BLI_assert(vsockets.size() == r_result.size());
    for (uint i = 0; i < vsockets.size(); i++) {
      r_result[i] = &this->lookup_socket(*vsockets[i]);
    }
  }

  void lookup_sockets(ArrayRef<const VInputSocket *> vsockets,
                      MutableArrayRef<const MFInputSocket *> r_result)
  {
    BLI_assert(vsockets.size() == r_result.size());
    for (uint i = 0; i < vsockets.size(); i++) {
      r_result[i] = &this->lookup_socket(*vsockets[i]);
    }
  }
};

}  // namespace FN

#endif /* __FN_VTREE_MULTI_FUNCTION_NETWORK_H__ */
