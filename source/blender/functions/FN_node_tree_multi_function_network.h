#ifndef __FN_VTREE_MULTI_FUNCTION_NETWORK_H__
#define __FN_VTREE_MULTI_FUNCTION_NETWORK_H__

#include "FN_node_tree.h"

#include "BLI_multi_map.h"
#include "BLI_index_to_ref_map.h"

#include "FN_multi_function_network.h"

namespace FN {

using BLI::IndexToRefMap;
using BLI::MultiMap;

class DummySocketMap {
 private:
  const FunctionNodeTree *m_function_tree;
  const MFNetwork *m_network;

  IndexToRefMap<const MFSocket> m_dummy_socket_by_fsocket_id;
  IndexToRefMap<const FSocket> m_fsocket_by_dummy_socket_id;

 public:
  DummySocketMap(const FunctionNodeTree &function_tree,
                 const MFNetwork &network,
                 IndexToRefMap<const MFSocket> dummy_socket_by_fsocket_id,
                 IndexToRefMap<const FSocket> fsocket_by_dummy_socket_id)
      : m_function_tree(&function_tree),
        m_network(&network),
        m_dummy_socket_by_fsocket_id(std::move(dummy_socket_by_fsocket_id)),
        m_fsocket_by_dummy_socket_id(std::move(fsocket_by_dummy_socket_id))
  {
  }

  bool is_mapped(const FSocket &fsocket) const
  {
    return m_dummy_socket_by_fsocket_id.contains(fsocket.id());
  }

  bool is_mapped(const MFSocket &socket) const
  {
    return m_fsocket_by_dummy_socket_id.contains(socket.id());
  }

  const MFInputSocket &lookup_singly_mapped_input_socket(const FInputSocket &fsocket) const
  {
    return m_dummy_socket_by_fsocket_id.lookup(fsocket.id()).as_input();
  }

  const MFOutputSocket &lookup_socket(const FOutputSocket &fsocket) const
  {
    return m_dummy_socket_by_fsocket_id.lookup(fsocket.id()).as_output();
  }

  const FInputSocket &lookup_fsocket(const MFInputSocket &socket) const
  {
    BLI_assert(socket.node().is_dummy());
    return m_fsocket_by_dummy_socket_id.lookup(socket.id()).as_input();
  }

  const FOutputSocket &lookup_fsocket(const MFOutputSocket &socket) const
  {
    BLI_assert(socket.node().is_dummy());
    return m_fsocket_by_dummy_socket_id.lookup(socket.id()).as_output();
  }
};

class FunctionTreeMFNetwork {
 private:
  const FunctionNodeTree &m_function_tree;
  std::unique_ptr<MFNetwork> m_network;
  DummySocketMap m_socket_map;

 public:
  FunctionTreeMFNetwork(const FunctionNodeTree &function_tree,
                        std::unique_ptr<MFNetwork> network,
                        DummySocketMap socket_map)
      : m_function_tree(function_tree),
        m_network(std::move(network)),
        m_socket_map(std::move(socket_map))
  {
  }

  const FunctionNodeTree &function_tree() const
  {
    return m_function_tree;
  }

  const MFNetwork &network() const
  {
    return *m_network;
  }

  bool is_mapped(const FSocket &fsocket) const
  {
    return m_socket_map.is_mapped(fsocket);
  }

  bool is_mapped(const MFSocket &socket) const
  {
    return m_socket_map.is_mapped(socket);
  }

  const MFInputSocket &lookup_dummy_socket(const FInputSocket &fsocket) const
  {
    const MFInputSocket &socket = m_socket_map.lookup_singly_mapped_input_socket(fsocket);
    BLI_assert(socket.node().is_dummy());
    return socket;
  }

  const MFOutputSocket &lookup_dummy_socket(const FOutputSocket &fsocket) const
  {
    const MFOutputSocket &socket = this->lookup_socket(fsocket);
    BLI_assert(socket.node().is_dummy());
    return socket;
  }

  const MFOutputSocket &lookup_socket(const FOutputSocket &fsocket) const
  {
    return m_socket_map.lookup_socket(fsocket);
  }

  const FInputSocket &lookup_fsocket(const MFInputSocket &socket) const
  {
    return m_socket_map.lookup_fsocket(socket);
  }

  const FOutputSocket &lookup_fsocket(const MFOutputSocket &socket) const
  {
    return m_socket_map.lookup_fsocket(socket);
  }

  void lookup_dummy_sockets(ArrayRef<const FOutputSocket *> fsockets,
                            MutableArrayRef<const MFOutputSocket *> r_result) const
  {
    BLI_assert(fsockets.size() == r_result.size());
    for (uint i : fsockets.index_range()) {
      r_result[i] = &this->lookup_socket(*fsockets[i]);
    }
  }

  void lookup_dummy_sockets(ArrayRef<const FInputSocket *> fsockets,
                            MutableArrayRef<const MFInputSocket *> r_result) const
  {
    BLI_assert(fsockets.size() == r_result.size());
    for (uint i : fsockets.index_range()) {
      r_result[i] = &this->lookup_dummy_socket(*fsockets[i]);
    }
  }
};

}  // namespace FN

#endif /* __FN_VTREE_MULTI_FUNCTION_NETWORK_H__ */
