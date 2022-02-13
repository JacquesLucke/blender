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

#pragma once

/** \file
 * \ingroup fn
 *
 * A `SGraph` ("socket graph") is a graph data structure wrapper. It wraps graphs where each node
 * has input and output sockets and links only exist between an input and output of two different
 * nodes.
 *
 * In itself, it can not hold a graph. Instead different concrete graph data structures can
 * implement a "SGraphAdapter". This allows generic algorithms to work on different concrete graph
 * data structures.
 *
 * Generic algorithms don't work on the adapter directly, but on SGraph<Adapter>, which adds some
 * utilities on top of the adapter. This minimizes the amount of functionality that has to be
 * implemented by an adapter, while still making generic algorithms somewhat convenient to write.
 */

#include "BLI_hash.hh"
#include "BLI_utildefines.h"

namespace blender::fn::sgraph {

template<typename SGraphAdapter> class SGraphT;
template<typename SGraphAdapter> class NodeT;
template<typename SGraphAdapter> class InSocketT;
template<typename SGraphAdapter> class OutSocketT;

template<typename SGraphAdapter> class NodeT {
 private:
  using NodeID = typename SGraphAdapter::NodeID;
  using SGraph = SGraphT<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;

  friend InSocket;
  friend OutSocket;

 public:
  NodeID id;

  NodeT(NodeID id) : id(std::move(id))
  {
  }

  int inputs_size(const SGraph &graph) const
  {
    return graph.adapter_->node_inputs_size(this->id);
  }

  int outputs_size(const SGraph &graph) const
  {
    return graph.adapter_->node_outputs_size(this->id);
  }

  InSocket input(const SGraph &graph, const int index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->inputs_size(graph));
    UNUSED_VARS_NDEBUG(graph);
    return {*this, index};
  }

  OutSocket output(const SGraph &graph, const int index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->outputs_size(graph));
    UNUSED_VARS_NDEBUG(graph);
    return {*this, index};
  }

  uint64_t hash() const
  {
    return get_default_hash(this->id);
  }

  friend bool operator==(const NodeT &a, const NodeT &b)
  {
    return a.id == b.id;
  }

  friend bool operator!=(const NodeT &a, const NodeT &b)
  {
    return !(a == b);
  }

  std::string debug_name(const SGraph &graph) const
  {
    return graph.adapter_->node_debug_name(this->id);
  }
};

template<typename SGraphAdapter> class InSocketT {
 private:
  using NodeID = typename SGraphAdapter::NodeID;
  using SGraph = SGraphT<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;
  using Node = NodeT<SGraphAdapter>;

 public:
  Node node;
  int index;

  template<typename F> void foreach_linked(const SGraph &graph, const F &f) const
  {
    graph.adapter_->foreach_linked_output(
        this->node.id, this->index, [&](const NodeID &linked_node, const int linked_index) {
          f(OutSocket{linked_node, linked_index});
        });
  }

  std::string debug_name(const SGraph &graph) const
  {
    return graph.adapter_->input_socket_debug_name(this->node.id, this->index);
  }

  friend bool operator==(const InSocketT &a, const InSocketT &b)
  {
    return a.node == b.node && a.index == b.index;
  }

  friend bool operator!=(const InSocketT &a, const InSocketT &b)
  {
    return !(a == b);
  }
};

template<typename SGraphAdapter> class OutSocketT {
 private:
  using NodeID = typename SGraphAdapter::NodeID;
  using SGraph = SGraphT<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;
  using Node = NodeT<SGraphAdapter>;

 public:
  Node node;
  int index;

  template<typename F> void foreach_linked(const SGraph &graph, const F &f) const
  {
    graph.adapter_->foreach_linked_input(
        this->node.id, this->index, [&](const NodeID &linked_node, const int linked_index) {
          f(InSocket{linked_node, linked_index});
        });
  }

  std::string debug_name(const SGraph &graph) const
  {
    return graph.adapter_->output_socket_debug_name(this->node.id, this->index);
  }

  friend bool operator==(const OutSocketT &a, const OutSocketT &b)
  {
    return a.node == b.node && a.index == b.index;
  }

  friend bool operator!=(const OutSocketT &a, const OutSocketT &b)
  {
    return !(a == b);
  }
};

template<typename SGraphAdapter> class SocketT {
 private:
  using NodeID = typename SGraphAdapter::NodeID;
  using SGraph = SGraphT<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;
  using Node = NodeT<SGraphAdapter>;

 public:
  Node node;
  int index;
  bool is_input;

  SocketT(Node node, int index, bool is_input) : node(node), index(index), is_input(is_input)
  {
  }

  SocketT(const InSocket socket) : SocketT(socket.node, socket.index, true)
  {
  }

  SocketT(const OutSocket socket) : SocketT(socket.node, socket.index, false)
  {
  }

  uint64_t hash() const
  {
    return get_default_hash_3(node, index, is_input);
  }

  friend bool operator==(const SocketT &a, const SocketT &b)
  {
    return a.node == b.node && a.index == b.index && a.is_input == b.is_input;
  }

  explicit operator InSocket() const
  {
    BLI_assert(is_input);
    return {node, index};
  }

  explicit operator OutSocket() const
  {
    BLI_assert(!is_input);
    return {node, index};
  }
};

template<typename SGraphAdapter> class LinkT {
 private:
  using NodeID = typename SGraphAdapter::NodeID;
  using SGraph = SGraphT<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;
  using Node = NodeT<SGraphAdapter>;

 public:
  OutSocket from;
  InSocket to;
};

template<typename SGraphAdapter> class SGraphT {
 public:
  using NodeID = typename SGraphAdapter::NodeID;
  using Node = NodeT<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;
  using Socket = SocketT<SGraphAdapter>;
  using Link = LinkT<SGraphAdapter>;

 private:
  friend Node;
  friend InSocket;
  friend OutSocket;
  friend Link;

  SGraphAdapter *adapter_;

 public:
  SGraphT(SGraphAdapter &adapter) : adapter_(&adapter)
  {
  }

  const SGraphAdapter adapter() const
  {
    return *adapter_;
  }

  template<typename F> void foreach_node(const F &f) const
  {
    adapter_->foreach_node([&](const NodeID &node_id) { f(Node(node_id)); });
  }

  template<typename F> void foreach_link(const F &f) const
  {
    this->foreach_node([&](const Node &node) {
      for (const int i : IndexRange(node.outputs_size(*this))) {
        OutSocket out_socket{node, i};
        out_socket.foreach_linked(*this, [&](const InSocket &in_socket) {
          f(Link{out_socket, in_socket});
        });
      }
    });
  }
};

}  // namespace blender::fn::sgraph
