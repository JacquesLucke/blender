#ifndef __FN_VTREE_MULTI_FUNCTION_NETWORK_H__
#define __FN_VTREE_MULTI_FUNCTION_NETWORK_H__

#include "FN_node_tree.h"

#include "BLI_multi_map.h"

#include "FN_multi_function_network.h"

namespace FN {

using BLI::MultiMap;

#define InlinedTreeMFSocketMap_UNMAPPED UINT_MAX
#define InlinedTreeMFSocketMap_MULTIMAPPED (UINT_MAX - 1)

class InlinedTreeMFSocketMap {
 private:
  /* An input fsocket can be mapped to multiple sockets.
   * An output fsocket can be mapped to at most one socket.
   */
  const FunctionNodeTree *m_function_tree;
  const MFNetwork *m_network;
  Array<uint> m_single_socket_by_fsocket;
  MultiMap<uint, uint> m_multiple_inputs_by_fsocket;
  Array<uint> m_fsocket_by_socket;

 public:
  InlinedTreeMFSocketMap(const FunctionNodeTree &function_tree,
                         const MFNetwork &network,
                         Array<uint> single_socket_by_fsocket,
                         MultiMap<uint, uint> multiple_inputs_by_fsocket,
                         Array<uint> fsocket_by_socket)
      : m_function_tree(&function_tree),
        m_network(&network),
        m_single_socket_by_fsocket(std::move(single_socket_by_fsocket)),
        m_multiple_inputs_by_fsocket(std::move(multiple_inputs_by_fsocket)),
        m_fsocket_by_socket(std::move(fsocket_by_socket))
  {
  }

  bool is_mapped(const FSocket &fsocket) const
  {
    return m_single_socket_by_fsocket[fsocket.id()] < InlinedTreeMFSocketMap_MULTIMAPPED;
  }

  bool is_mapped(const MFSocket &socket) const
  {
    return m_fsocket_by_socket[socket.id()] != InlinedTreeMFSocketMap_UNMAPPED;
  }

  const MFInputSocket &lookup_singly_mapped_input_socket(const FInputSocket &fsocket) const
  {
    BLI_assert(this->lookup_socket(fsocket).size() == 1);
    uint mapped_id = m_single_socket_by_fsocket[fsocket.id()];
    return m_network->socket_by_id(mapped_id).as_input();
  }

  Vector<const MFInputSocket *> lookup_socket(const FInputSocket &fsocket) const
  {
    uint id = fsocket.id();
    uint mapped_value = m_single_socket_by_fsocket[id];
    switch (mapped_value) {
      case InlinedTreeMFSocketMap_UNMAPPED: {
        return {};
      }
      case InlinedTreeMFSocketMap_MULTIMAPPED: {
        Vector<const MFInputSocket *> sockets;
        for (uint mapped_id : m_multiple_inputs_by_fsocket.lookup(id)) {
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

  const MFOutputSocket &lookup_socket(const FOutputSocket &fsocket) const
  {
    uint mapped_id = m_single_socket_by_fsocket[fsocket.id()];
    return m_network->socket_by_id(mapped_id).as_output();
  }

  const FInputSocket &lookup_fsocket(const MFInputSocket &socket) const
  {
    uint mapped_id = m_fsocket_by_socket[socket.id()];
    return m_function_tree->socket_by_id(mapped_id).as_input();
  }

  const FOutputSocket &lookup_fsocket(const MFOutputSocket &socket) const
  {
    uint mapped_id = m_fsocket_by_socket[socket.id()];
    return m_function_tree->socket_by_id(mapped_id).as_output();
  }
};

class FunctionTreeMFNetwork {
 private:
  const FunctionNodeTree &m_function_tree;
  std::unique_ptr<MFNetwork> m_network;
  InlinedTreeMFSocketMap m_socket_map;

 public:
  FunctionTreeMFNetwork(const FunctionNodeTree &function_tree,
                        std::unique_ptr<MFNetwork> network,
                        InlinedTreeMFSocketMap socket_map)
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
