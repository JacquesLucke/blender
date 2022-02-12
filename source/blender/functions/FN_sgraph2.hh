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
 */

#include "BLI_function_ref.hh"
#include "BLI_span.hh"

namespace blender::fn::sgraph2 {

struct SGraphTypes {
  using NodeID = void;
  using LinkID = void;
};

template<typename Types> class SGraph;
template<typename Types> class NodeT;
template<typename Types> class InSocketT;
template<typename Types> class OutSocketT;
template<typename Types> class SocketT;

template<typename Types> class NodeT {
 public:
  using NodeID = typename Types::NodeID;
  using LinkID = typename Types::LinkID;

  NodeID id;

  NodeT() = default;
  NodeT(NodeID id) : id(std::move(id))
  {
  }

  int inputs_size(const SGraph<Types> &graph) const
  {
    return graph.inputs_size(this->id);
  }

  int outputs_size(const SGraph<Types> &graph) const
  {
    return graph.outputs_size(this->id);
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

  std::string debug_name(const SGraph<Types> &graph) const
  {
    return graph.debug_name(*this);
  }
};

template<typename Types> class InSocketT {
 public:
  using NodeID = typename Types::NodeID;
  using LinkID = typename Types::LinkID;

  NodeT<Types> node;
  int index;

  InSocketT() = default;
  InSocketT(NodeT<Types> node, const int index) : node(std::move(node)), index(index)
  {
  }

  std::string debug_name(const SGraph<Types> &graph) const
  {
    return graph.debug_name(*this);
  }
};

template<typename Types> class OutSocketT {
 public:
  using NodeID = typename Types::NodeID;
  using LinkID = typename Types::LinkID;

  NodeT<Types> node;
  int index;

  OutSocketT() = default;
  OutSocketT(NodeT<Types> node, const int index) : node(std::move(node)), index(index)
  {
  }

  void foreach_link(
      const SGraph<Types> &graph,
      const FunctionRef<void(const InSocketT<Types> &, const LinkID &link_id)> fn) const
  {
    graph.foreach_link(*this, fn);
  }

  std::string debug_name(const SGraph<Types> &graph) const
  {
    return graph.debug_name(*this);
  }
};

enum class SocketInOut {
  In,
  Out,
};

template<typename Types> class SocketT {
 public:
  using NodeID = typename Types::NodeID;
  using LinkID = typename Types::LinkID;

  NodeT<Types> node;
  int index;
  SocketInOut in_out;

  SocketT() = default;
  SocketT(NodeT<Types> node, const int index, const SocketInOut in_out)
      : node(std::move(node)), index(index), in_out(in_out)
  {
  }
  SocketT(InSocketT<Types> socket) : SocketT(socket.node, socket.index, SocketInOut::In)
  {
  }
  SocketT(OutSocketT<Types> socket) : SocketT(socket.node, socket.index, SocketInOut::Out)
  {
  }
  explicit operator InSocketT<Types>() const
  {
    BLI_assert(in_out == SocketInOut::In);
    return {node, index};
  }
  explicit operator OutSocketT<Types>() const
  {
    BLI_assert(in_out == SocketInOut::Out);
    return {node, index};
  }

  std::string debug_name(const SGraph<Types> &graph) const
  {
    if (this->in_out == SocketInOut::In) {
      return InSocketT<Types>(*this).debug_name(graph);
    }
    return OutSocketT<Types>(*this).debug_name(graph);
  }
};

template<typename Types> class LinkT {
 public:
  using NodeID = typename Types::NodeID;
  using LinkID = typename Types::LinkID;

  OutSocketT<Types> from;
  InSocketT<Types> to;
  LinkID id;

  LinkT() = default;
  LinkT(OutSocketT<Types> from, InSocketT<Types> to, LinkID id)
      : from(std::move(from)), to(std::move(to)), id(std::move(id))
  {
  }
};

template<typename Types> class SGraph {
 public:
  using NodeID = typename Types::NodeID;
  using LinkID = typename Types::LinkID;
  using Node = NodeT<Types>;
  using InSocket = InSocketT<Types>;
  using OutSocket = OutSocketT<Types>;
  using Socket = SocketT<Types>;
  using Link = LinkT<Types>;

  void foreach_link(const FunctionRef<void(Link)> fn) const
  {
    this->foreach_node([&](const Node &node) {
      for (const int i : IndexRange(node.outputs_size(*this))) {
        OutSocket from_socket{node, i};
        from_socket.foreach_link([&](const InSocket &to_socket, const LinkID &link_id) {
          fn(Link(from_socket, to_socket, link_id));
        });
      }
    });
  }

  void foreach_node(const FunctionRef<void(const Node &)> fn) const
  {
    this->foreach_node([&](const NodeID &node_id) { fn(Node(node_id)); });
  }

 private:
  virtual void foreach_node(const FunctionRef<void(const NodeID &)> fn) const = 0;
  virtual int inputs_size(const NodeID &node_id) const = 0;
  virtual int outputs_size(const NodeID &node_id) const = 0;
  virtual void foreach_link(
      InSocket to_socket,
      FunctionRef<void(const OutSocket &from, const LinkID &link_id)> fn) const = 0;
  virtual void foreach_link(
      OutSocket from_socket,
      FunctionRef<void(const InSocket &to, const LinkID &link_id)> fn) const = 0;

  virtual std::string debug_name(const Node &node) const
  {
    std::stringstream ss;
    ss << node.id;
    return ss.str();
  }

  virtual std::string debug_name(const InSocket &socket) const
  {
    std::stringstream ss;
    ss << socket.node.id << ":IN:" << socket.index;
    return ss.str();
  }

  virtual std::string debug_name(const OutSocket &socket) const
  {
    std::stringstream ss;
    ss << socket.node.id << ":OUT:" << socket.index;
    return ss.str();
  }
};

}  // namespace blender::fn::sgraph2
