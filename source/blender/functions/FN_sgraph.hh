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

#include "BLI_assert.h"
#include "BLI_dot_export.hh"

namespace blender::fn::sgraph {

template<typename SGraphAdapter> class SGraph;
template<typename SGraphAdapter> class NodeT;
template<typename SGraphAdapter> class InSocketT;
template<typename SGraphAdapter> class OutSocketT;

template<typename SGraphAdapter> class NodeT {
 private:
  using NodeID = typename SGraphAdapter::NodeID;
  using OwnerSGraph = SGraph<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;

  friend InSocket;
  friend OutSocket;

  NodeID id_;

 public:
  NodeT(NodeID id) : id_(std::move(id))
  {
  }

  int inputs_size(const OwnerSGraph &graph) const
  {
    return graph.adapter_->node_inputs_size(id_);
  }

  int outputs_size(const OwnerSGraph &graph) const
  {
    return graph.adapter_->node_outputs_size(id_);
  }

  InSocket input(const OwnerSGraph &graph, const int index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->inputs_size(graph));
    UNUSED_VARS_NDEBUG(graph);
    return {*this, index};
  }

  OutSocket output(const OwnerSGraph &graph, const int index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->outputs_size(graph));
    UNUSED_VARS_NDEBUG(graph);
    return {*this, index};
  }

  uint64_t hash() const
  {
    return get_default_hash(id_);
  }

  friend bool operator==(const NodeT &a, const NodeT &b)
  {
    return a.id_ == b.id_;
  }

  friend bool operator!=(const NodeT &a, const NodeT &b)
  {
    return !(a == b);
  }

  std::string debug_name(const OwnerSGraph &graph) const
  {
    return graph.adapter_->node_debug_name(id_);
  }
};

template<typename SGraphAdapter> class InSocketT {
 private:
  using NodeID = typename SGraphAdapter::NodeID;
  using OwnerSGraph = SGraph<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;
  using Node = NodeT<SGraphAdapter>;

 public:
  Node node;
  int index;

  template<typename F> void foreach_linked(const OwnerSGraph &graph, const F &f) const
  {
    graph.adapter_->foreach_linked_output(
        this->node.id_, this->index, [&](const NodeID &linked_node, const int linked_index) {
          f(OutSocket{linked_node, linked_index});
        });
  }

  std::string debug_name(const OwnerSGraph &graph) const
  {
    return graph.adapter_->input_socket_debug_name(this->node.id_, this->index);
  }
};

template<typename SGraphAdapter> class OutSocketT {
 private:
  using NodeID = typename SGraphAdapter::NodeID;
  using OwnerSGraph = SGraph<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;
  using Node = NodeT<SGraphAdapter>;

 public:
  Node node;
  int index;

  template<typename F> void foreach_linked(const OwnerSGraph &graph, const F &f) const
  {
    graph.adapter_->foreach_linked_input(
        this->node.id_, this->index, [&](const NodeID &linked_node, const int linked_index) {
          f(InSocket{linked_node, linked_index});
        });
  }

  std::string debug_name(const OwnerSGraph &graph) const
  {
    return graph.adapter_->output_socket_debug_name(this->node.id_, this->index);
  }
};

template<typename SGraphAdapter> class LinkT {
 private:
  using NodeID = typename SGraphAdapter::NodeID;
  using OwnerSGraph = SGraph<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;
  using Node = NodeT<SGraphAdapter>;

 public:
  InSocket in_socket;
  OutSocket out_socket;
};

template<typename SGraphAdapter> class SGraph {
 public:
  using NodeID = typename SGraphAdapter::NodeID;
  using Node = NodeT<SGraphAdapter>;
  using InSocket = InSocketT<SGraphAdapter>;
  using OutSocket = OutSocketT<SGraphAdapter>;
  using Link = LinkT<SGraphAdapter>;

 private:
  friend Node;
  friend InSocket;
  friend OutSocket;
  friend Link;

  SGraphAdapter *adapter_;

 public:
  SGraph(SGraphAdapter &adapter) : adapter_(&adapter)
  {
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
          f(Link{in_socket, out_socket});
        });
      }
    });
  }
};

template<typename SGraphAdapter>
inline std::string sgraph_to_dot(const SGraph<SGraphAdapter> &graph)
{
  using SGraph_ = SGraph<SGraphAdapter>;
  using Node = typename SGraph_::Node;
  using Link = typename SGraph_::Link;

  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<Node, dot::NodeWithSocketsRef> dot_nodes;

  graph.foreach_node([&](const Node &node) {
    dot::Node &dot_node = digraph.new_node("");
    Vector<std::string> input_names;
    Vector<std::string> output_names;
    for (const int i : IndexRange(node.inputs_size(graph))) {
      input_names.append(node.input(graph, i).debug_name(graph));
    }
    for (const int i : IndexRange(node.outputs_size(graph))) {
      output_names.append(node.output(graph, i).debug_name(graph));
    }
    dot_nodes.add_new(
        node,
        dot::NodeWithSocketsRef(dot_node, node.debug_name(graph), input_names, output_names));
  });

  graph.foreach_link([&](const Link &link) {
    dot::NodeWithSocketsRef &from_dot_node = dot_nodes.lookup(link.out_socket.node);
    dot::NodeWithSocketsRef &to_dot_node = dot_nodes.lookup(link.in_socket.node);
    digraph.new_edge(from_dot_node.output(link.out_socket.index),
                     to_dot_node.input(link.in_socket.index));
  });

  return digraph.to_dot_string();
}

}  // namespace blender::fn::sgraph
