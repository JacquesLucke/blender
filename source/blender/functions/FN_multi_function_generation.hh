/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __FN_MULTI_FUNCTION_GENERATION_HH__
#define __FN_MULTI_FUNCTION_GENERATION_HH__

#include "BKE_derived_node_tree.hh"
#include "BLI_resource_collector.hh"
#include "FN_multi_function_network.hh"

namespace blender {
namespace fn {

inline bool is_data_socket(const bNodeSocket *UNUSED(bsocket))
{
  /* TODO */
  return true;
}

class MFSocketByDSocketMap {
 private:
  Array<Vector<MFSocket *, 1>> m_sockets_by_dsocket_id;
  Array<MFOutputSocket *> m_socket_by_group_input_id;

 public:
  MFSocketByDSocketMap(const BKE::DerivedNodeTree &tree)
      : m_sockets_by_dsocket_id(tree.sockets().size()),
        m_socket_by_group_input_id(tree.group_inputs().size(), nullptr)
  {
  }

  void add(const BKE::DSocket &dsocket, MFSocket &socket)
  {
    BLI_assert(dsocket.is_input() == socket.is_input());
    m_sockets_by_dsocket_id[dsocket.id()].append(&socket);
  }

  void add(const BKE::DInputSocket &dsocket, MFInputSocket &socket)
  {
    m_sockets_by_dsocket_id[dsocket.id()].append(&socket);
  }

  void add(const BKE::DOutputSocket &dsocket, MFOutputSocket &socket)
  {
    m_sockets_by_dsocket_id[dsocket.id()].append(&socket);
  }

  void add(Span<const BKE::DInputSocket *> dsockets, Span<MFInputSocket *> sockets)
  {
    assert_same_size(dsockets, sockets);
    for (uint i : dsockets.index_range()) {
      this->add(*dsockets[i], *sockets[i]);
    }
  }

  void add(Span<const BKE::DOutputSocket *> dsockets, Span<MFOutputSocket *> sockets)
  {
    assert_same_size(dsockets, sockets);
    for (uint i : dsockets.index_range()) {
      this->add(*dsockets[i], *sockets[i]);
    }
  }

  void add(const BKE::DGroupInput &group_input, MFOutputSocket &socket)
  {
    BLI_assert(m_socket_by_group_input_id[group_input.id()] == nullptr);
    m_socket_by_group_input_id[group_input.id()] = &socket;
  }

  void add_try_match(const BKE::DNode &dnode, MFNode &node)
  {
    this->add_try_match(dnode.inputs(), node.inputs());
    this->add_try_match(dnode.outputs(), node.outputs());
  }

  void add_try_match(Span<const BKE::DSocket *> dsockets, Span<MFSocket *> sockets)
  {
    uint used_sockets = 0;
    for (const BKE::DSocket *dsocket : dsockets) {
      bNodeSocket *bsocket = dsocket->socket_ref().bsocket();
      if (bsocket->flag & SOCK_UNAVAIL) {
        continue;
      }
      if (!is_data_socket(bsocket)) {
        continue;
      }
      MFSocket *socket = sockets[used_sockets];
      this->add(*dsocket, *socket);
      used_sockets++;
    }
  }

  MFOutputSocket &lookup(const BKE::DGroupInput &group_input)
  {
    MFOutputSocket *socket = m_socket_by_group_input_id[group_input.id()];
    BLI_assert(socket != nullptr);
    return *socket;
  }

  MFOutputSocket &lookup(const BKE::DOutputSocket &dsocket)
  {
    auto &sockets = m_sockets_by_dsocket_id[dsocket.id()];
    BLI_assert(sockets.size() == 1);
    return sockets[0]->as_output();
  }

  Span<MFInputSocket *> lookup(const BKE::DInputSocket &dsocket)
  {
    return m_sockets_by_dsocket_id[dsocket.id()].as_span().cast<MFInputSocket *>();
  }

  bool is_mapped(const BKE::DSocket &dsocket) const
  {
    return m_sockets_by_dsocket_id[dsocket.id()].size() >= 1;
  }
};

class NodeMFNetworkBuilder {
 private:
  ResourceCollector &m_resources;
  MFNetwork &m_network;
  MFSocketByDSocketMap &m_socket_map;
  const BKE::DNode &m_node;

 public:
  NodeMFNetworkBuilder(ResourceCollector &resources,
                       MFNetwork &network,
                       MFSocketByDSocketMap &socket_map,
                       const BKE::DNode &node)
      : m_resources(resources), m_network(network), m_socket_map(socket_map), m_node(node)
  {
  }

  void add_link(MFOutputSocket &from, MFInputSocket &to)
  {
    m_network.add_link(from, to);
  }

  MFFunctionNode &add_function(const MultiFunction &function)
  {
    return m_network.add_function(function);
  }

  template<typename T, typename... Args> T &construct_fn(Args &&... args)
  {
    BLI_STATIC_ASSERT((std::is_base_of_v<MultiFunction, T>), "");
    void *buffer = m_resources.allocate(sizeof(T), alignof(T));
    T *fn = new (buffer) T(std::forward<Args>(args)...);
    m_resources.add(destruct_ptr<T>(fn), fn->name().data());
    return *fn;
  }

  void set_matching_fn(const MultiFunction &function)
  {
    MFFunctionNode &node = this->add_function(function);
    m_socket_map.add_try_match(m_node, node);
  }

  bNode &bnode()
  {
    return *m_node.node_ref().bnode();
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_MULTI_FUNCTION_GENERATION_HH__ */
