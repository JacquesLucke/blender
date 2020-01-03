#ifndef __FN_VTREE_MULTI_FUNCTION_NETWORK_H__
#define __FN_VTREE_MULTI_FUNCTION_NETWORK_H__

#include "FN_node_tree.h"

#include "BLI_multi_map.h"
#include "BLI_index_map.h"

#include "FN_multi_function_network.h"

namespace FN {

using BLI::IndexMap;
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
  const FunctionNodeTree *m_function_tree;
  const MFNetwork *m_network;

  IndexMap<const MFSocket *> m_dummy_socket_by_fsocket_id;
  IndexMap<const FSocket *> m_fsocket_by_dummy_socket_id;

 public:
  InlinedTreeMFSocketMap(const FunctionNodeTree &function_tree,
                         const MFNetwork &network,
                         IndexMap<const MFSocket *> dummy_socket_by_fsocket_id,
                         IndexMap<const FSocket *> fsocket_by_dummy_socket_id)
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
    return m_dummy_socket_by_fsocket_id.lookup(fsocket.id())->as_input();
  }

  const MFOutputSocket &lookup_socket(const FOutputSocket &fsocket) const
  {
    return m_dummy_socket_by_fsocket_id.lookup(fsocket.id())->as_output();
  }

  const FInputSocket &lookup_fsocket(const MFInputSocket &socket) const
  {
    BLI_assert(socket.node().is_dummy());
    return m_fsocket_by_dummy_socket_id.lookup(socket.id())->as_input();
  }

  const FOutputSocket &lookup_fsocket(const MFOutputSocket &socket) const
  {
    BLI_assert(socket.node().is_dummy());
    return m_fsocket_by_dummy_socket_id.lookup(socket.id())->as_output();
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
