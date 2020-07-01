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

#ifndef __BKE_NODE_TREE_FUNCTION_HH__
#define __BKE_NODE_TREE_FUNCTION_HH__

#include "FN_multi_function_builder.hh"
#include "FN_multi_function_network.hh"

#include "BKE_derived_node_tree.hh"

#include "BLI_resource_collector.hh"

namespace blender {
namespace bke {

/* Maybe this should be moved to BKE_node.h. */
inline bool is_multi_function_data_socket(const bNodeSocket *bsocket)
{
  if (bsocket->typeinfo->get_mf_data_type != nullptr) {
    BLI_assert(bsocket->typeinfo->build_mf_network != nullptr);
    return true;
  }
  return false;
}

/* Maybe this should be moved to BKE_node.h. */
inline std::optional<fn::MFDataType> try_get_multi_function_data_type_of_socket(
    const bNodeSocket *bsocket)
{
  if (bsocket->typeinfo->get_mf_data_type == nullptr) {
    return {};
  }
  return bsocket->typeinfo->get_mf_data_type();
}

/**
 * A MFNetworkTreeMap maps various various components of a bke::DerivedNodeTree to components of a
 * fn::MFNetwork. This is necessary for further processing of a multi-function network that has
 * been generated from a node tree.
 */
class MFNetworkTreeMap {
 private:
  /**
   * Store by id instead of using a hash table to avoid unnecessary hash table lookups.
   *
   * Input sockets in a node tree can have multiple corresponding sockets in the generated
   * MFNetwork. This is because nodes are allowed to expand into multiple multi-function nodes.
   */
  Array<Vector<fn::MFSocket *, 1>> m_sockets_by_dsocket_id;
  Array<fn::MFOutputSocket *> m_socket_by_group_input_id;

 public:
  MFNetworkTreeMap(const DerivedNodeTree &tree)
      : m_sockets_by_dsocket_id(tree.sockets().size()),
        m_socket_by_group_input_id(tree.group_inputs().size(), nullptr)
  {
  }

  void add(const DSocket &dsocket, fn::MFSocket &socket)
  {
    BLI_assert(dsocket.is_input() == socket.is_input());
    m_sockets_by_dsocket_id[dsocket.id()].append(&socket);
  }

  void add(const DInputSocket &dsocket, fn::MFInputSocket &socket)
  {
    m_sockets_by_dsocket_id[dsocket.id()].append(&socket);
  }

  void add(const DOutputSocket &dsocket, fn::MFOutputSocket &socket)
  {
    m_sockets_by_dsocket_id[dsocket.id()].append(&socket);
  }

  void add(Span<const DInputSocket *> dsockets, Span<fn::MFInputSocket *> sockets)
  {
    assert_same_size(dsockets, sockets);
    for (uint i : dsockets.index_range()) {
      this->add(*dsockets[i], *sockets[i]);
    }
  }

  void add(Span<const DOutputSocket *> dsockets, Span<fn::MFOutputSocket *> sockets)
  {
    assert_same_size(dsockets, sockets);
    for (uint i : dsockets.index_range()) {
      this->add(*dsockets[i], *sockets[i]);
    }
  }

  void add(const DGroupInput &group_input, fn::MFOutputSocket &socket)
  {
    BLI_assert(m_socket_by_group_input_id[group_input.id()] == nullptr);
    m_socket_by_group_input_id[group_input.id()] = &socket;
  }

  void add_try_match(const DNode &dnode, fn::MFNode &node)
  {
    this->add_try_match(dnode.inputs(), node.inputs());
    this->add_try_match(dnode.outputs(), node.outputs());
  }

  void add_try_match(Span<const DSocket *> dsockets, Span<fn::MFSocket *> sockets)
  {
    uint used_sockets = 0;
    for (const DSocket *dsocket : dsockets) {
      if (!dsocket->is_available()) {
        continue;
      }
      if (!is_multi_function_data_socket(dsocket->bsocket())) {
        continue;
      }
      fn::MFSocket *socket = sockets[used_sockets];
      this->add(*dsocket, *socket);
      used_sockets++;
    }
  }

  fn::MFOutputSocket &lookup(const DGroupInput &group_input)
  {
    fn::MFOutputSocket *socket = m_socket_by_group_input_id[group_input.id()];
    BLI_assert(socket != nullptr);
    return *socket;
  }

  fn::MFOutputSocket &lookup(const DOutputSocket &dsocket)
  {
    auto &sockets = m_sockets_by_dsocket_id[dsocket.id()];
    BLI_assert(sockets.size() == 1);
    return sockets[0]->as_output();
  }

  Span<fn::MFInputSocket *> lookup(const DInputSocket &dsocket)
  {
    return m_sockets_by_dsocket_id[dsocket.id()].as_span().cast<fn::MFInputSocket *>();
  }

  fn::MFInputSocket &lookup_dummy(const DInputSocket &dsocket)
  {
    Span<fn::MFInputSocket *> sockets = this->lookup(dsocket);
    BLI_assert(sockets.size() == 1);
    fn::MFInputSocket &socket = *sockets[0];
    BLI_assert(socket.node().is_dummy());
    return socket;
  }

  fn::MFOutputSocket &lookup_dummy(const DOutputSocket &dsocket)
  {
    fn::MFOutputSocket &socket = this->lookup(dsocket);
    BLI_assert(socket.node().is_dummy());
    return socket;
  }

  bool is_mapped(const DSocket &dsocket) const
  {
    return m_sockets_by_dsocket_id[dsocket.id()].size() >= 1;
  }
};

struct CommonMFNetworkBuilderData {
  ResourceCollector &resources;
  fn::MFNetwork &network;
  MFNetworkTreeMap &network_map;
  const DerivedNodeTree &tree;
};

class MFNetworkBuilderBase {
 protected:
  CommonMFNetworkBuilderData &m_common;

 public:
  MFNetworkBuilderBase(CommonMFNetworkBuilderData &common) : m_common(common)
  {
  }

  void add_link(fn::MFOutputSocket &from, fn::MFInputSocket &to)
  {
    m_common.network.add_link(from, to);
  }

  fn::MFFunctionNode &add_function(const fn::MultiFunction &function)
  {
    return m_common.network.add_function(function);
  }

  template<typename T, typename... Args> T &construct_fn(Args &&... args)
  {
    BLI_STATIC_ASSERT((std::is_base_of_v<fn::MultiFunction, T>), "");
    void *buffer = m_common.resources.linear_allocator().allocate(sizeof(T), alignof(T));
    T *fn = new (buffer) T(std::forward<Args>(args)...);
    m_common.resources.add(destruct_ptr<T>(fn), fn->name().data());
    return *fn;
  }
};

class SocketMFNetworkBuilder : public MFNetworkBuilderBase {
 private:
  const DSocket *m_dsocket = nullptr;
  const DGroupInput *m_group_input = nullptr;
  bNodeSocket *m_bsocket;
  fn::MFOutputSocket *m_built_socket = nullptr;

 public:
  SocketMFNetworkBuilder(CommonMFNetworkBuilderData &common, const DSocket &dsocket)
      : MFNetworkBuilderBase(common), m_dsocket(&dsocket), m_bsocket(dsocket.bsocket())
  {
  }

  SocketMFNetworkBuilder(CommonMFNetworkBuilderData &common, const DGroupInput &group_input)
      : MFNetworkBuilderBase(common), m_group_input(&group_input), m_bsocket(group_input.bsocket())
  {
  }

  bNodeSocket &bsocket()
  {
    return *m_bsocket;
  }

  template<typename T> T *socket_default_value()
  {
    return (T *)m_bsocket->default_value;
  }

  template<typename T> void set_constant_value(T &value)
  {
    const fn::MultiFunction &fn = this->construct_fn<fn::CustomMF_Constant<T>>(std::move(value));
    this->set_generator_fn(fn);
  }

  void set_generator_fn(const fn::MultiFunction &fn)
  {
    fn::MFFunctionNode &node = this->add_function(fn);
    this->set_socket(node.output(0));
  }

  void set_socket(fn::MFOutputSocket &socket)
  {
    m_built_socket = &socket;
  }

  fn::MFOutputSocket *built_socket()
  {
    return m_built_socket;
  }
};

class NodeMFNetworkBuilder : public MFNetworkBuilderBase {
 private:
  const DNode &m_node;

 public:
  NodeMFNetworkBuilder(CommonMFNetworkBuilderData &common, const DNode &node)
      : MFNetworkBuilderBase(common), m_node(node)
  {
  }

  template<typename T, typename... Args> void construct_and_set_matching_fn(Args &&... args)
  {
    const fn::MultiFunction &function = this->construct_fn<T>(std::forward<Args>(args)...);
    this->set_matching_fn(function);
  }

  void set_matching_fn(const fn::MultiFunction &function)
  {
    fn::MFFunctionNode &node = this->add_function(function);
    m_common.network_map.add_try_match(m_node, node);
  }

  bNode &bnode()
  {
    return *m_node.node_ref().bnode();
  }

  const DNode &dnode() const
  {
    return m_node;
  }
};

MFNetworkTreeMap insert_node_tree_into_mf_network(fn::MFNetwork &network,
                                                  const DerivedNodeTree &tree,
                                                  ResourceCollector &resources);

}  // namespace bke
}  // namespace blender

#endif /* __BKE_NODE_TREE_FUNCTION_HH__ */
