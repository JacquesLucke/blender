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
  Map<const BKE::DSocket *, MFSocket *> &m_dummy_socket_map;
  const BKE::DNode &m_node;

 public:
  void add_link(MFOutputSocket &from, MFInputSocket &to)
  {
    m_network.add_link(from, to);
  }

  MFFunctionNode &add_function(const MultiFunction &function)
  {
    return m_network.add_function(function);
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_MULTI_FUNCTION_GENERATION_HH__ */
