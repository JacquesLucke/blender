#ifndef __FN_VTREE_MULTI_FUNCTION_NETWORK_H__
#define __FN_VTREE_MULTI_FUNCTION_NETWORK_H__

#include "FN_node_tree.h"

#include "BLI_multi_map.h"

#include "FN_multi_function_network.h"

namespace FN {

using BLI::MultiMap;

#define IdMultiMap_UNMAPPED UINT_MAX
#define IdMultiMap_MULTIMAPPED (UINT_MAX - 1)

class IdMultiMap {
 private:
  Array<uint> m_single_mapping;
  MultiMap<uint, uint> m_fallback_multimap;

 public:
  IdMultiMap(uint max_key_id) : m_single_mapping(max_key_id, IdMultiMap_UNMAPPED)
  {
  }

  bool contains(uint key_id) const
  {
    return m_single_mapping[key_id] != IdMultiMap_UNMAPPED;
  }

  ArrayRef<uint> lookup(uint key_id) const
  {
    const uint &stored_value = m_single_mapping[key_id];
    switch (stored_value) {
      case IdMultiMap_UNMAPPED: {
        return {};
      }
      case IdMultiMap_MULTIMAPPED: {
        return m_fallback_multimap.lookup(key_id);
      }
      default:
        return ArrayRef<uint>(&stored_value, 1);
    }
  }

  uint lookup_single(uint key_id) const
  {
    uint stored_value = m_single_mapping[key_id];
    BLI_assert(stored_value != IdMultiMap_UNMAPPED && stored_value != IdMultiMap_MULTIMAPPED);
    return stored_value;
  }

  void add(uint key_id, uint value_id)
  {
    uint &stored_value = m_single_mapping[key_id];
    switch (stored_value) {
      case IdMultiMap_UNMAPPED: {
        stored_value = value_id;
        break;
      }
      case IdMultiMap_MULTIMAPPED: {
        m_fallback_multimap.add(key_id, value_id);
        break;
      }
      default: {
        uint other_value_id = stored_value;
        stored_value = IdMultiMap_MULTIMAPPED;
        m_fallback_multimap.add_multiple_new(key_id, {other_value_id, value_id});
        break;
      }
    }
  }
};

class InlinedTreeMFSocketMap {
 private:
  /* An input fsocket can be mapped to multiple sockets.
   * An output fsocket can be mapped to at most one socket.
   */
  const FunctionNodeTree *m_function_tree;
  const MFNetwork *m_network;
  IdMultiMap m_socket_by_fsocket;
  Array<uint> m_fsocket_by_socket;

 public:
  InlinedTreeMFSocketMap(const FunctionNodeTree &function_tree,
                         const MFNetwork &network,
                         IdMultiMap socket_by_fsocket,
                         Array<uint> fsocket_by_socket)
      : m_function_tree(&function_tree),
        m_network(&network),
        m_socket_by_fsocket(std::move(socket_by_fsocket)),
        m_fsocket_by_socket(std::move(fsocket_by_socket))
  {
  }

  bool is_mapped(const FSocket &fsocket) const
  {
    return m_socket_by_fsocket.contains(fsocket.id());
  }

  bool is_mapped(const MFSocket &socket) const
  {
    return m_fsocket_by_socket[socket.id()] != IdMultiMap_UNMAPPED;
  }

  const MFInputSocket &lookup_singly_mapped_input_socket(const FInputSocket &fsocket) const
  {
    uint mapped_id = m_socket_by_fsocket.lookup_single(fsocket.id());
    return m_network->socket_by_id(mapped_id).as_input();
  }

  const MFOutputSocket &lookup_socket(const FOutputSocket &fsocket) const
  {
    uint mapped_id = m_socket_by_fsocket.lookup_single(fsocket.id());
    return m_network->socket_by_id(mapped_id).as_output();
  }

  const FInputSocket &lookup_fsocket(const MFInputSocket &socket) const
  {
    BLI_assert(socket.node().is_dummy());
    uint mapped_id = m_fsocket_by_socket[socket.id()];
    return m_function_tree->socket_by_id(mapped_id).as_input();
  }

  const FOutputSocket &lookup_fsocket(const MFOutputSocket &socket) const
  {
    BLI_assert(socket.node().is_dummy());
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
