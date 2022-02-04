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

namespace blender::fn {

template<typename Derived> struct NodeGraphAdapterBase {

  template<typename F> void foreach_link(const F &f) const
  {
    using NodeID = typename Derived::NodeID;

    const Derived &graph = this->derived_graph();
    graph.foreach_node([&](const NodeID &from_node) {
      for (const int from_index : IndexRange(graph.node_outputs_size(from_node))) {
        graph.foreach_linked_input(
            from_node, from_index, [&](const NodeID &to_node, const int to_index) {
              f(from_node, from_index, to_node, to_index);
            });
      }
    });
  }

 private:
  const Derived &derived_graph() const
  {
    return static_cast<const Derived &>(*this);
  }
};

}  // namespace blender::fn

namespace blender::fn::node_graph_example {

struct ExampleNodeGraphAdapter : public NodeGraphAdapterBase<ExampleNodeGraphAdapter> {
  using NodeID = int;

  int node_inputs_size(const NodeID &node) const
  {
    switch (node) {
      case 1:
        return 2;
      case 2:
        return 2;
      case 3:
        return 3;
    }
    BLI_assert_unreachable();
    return 0;
  }

  int node_outputs_size(const NodeID &node) const
  {
    switch (node) {
      case 1:
        return 2;
      case 2:
        return 1;
      case 3:
        return 3;
    }
    BLI_assert_unreachable();
    return 0;
  }

  template<typename F> void foreach_node(const F &f) const
  {
    f(1);
    f(2);
    f(3);
  }

  template<typename F>
  void foreach_linked_input(const NodeID &node, const int output_socket_index, const F &f) const
  {
    switch (node * 1000 + output_socket_index) {
      case 1000:
        f(2, 0);
        break;
      case 1001:
        f(2, 1);
        f(3, 2);
        break;
      case 2000:
        f(3, 0);
        break;
      default:
        break;
    }
  }

  template<typename F>
  void foreach_linked_output(const NodeID &node, const int input_socket_index, const F &f) const
  {
    switch (node * 1000 + input_socket_index) {
      case 2000:
        f(1, 0);
        break;
      case 2001:
        f(1, 1);
        break;
      case 3000:
        f(2, 0);
        break;
      case 3002:
        f(1, 1);
        break;
      default:
        break;
    }
  }

  std::string node_debug_name(const NodeID &node) const
  {
    return std::to_string(node);
  }

  std::string input_socket_debug_name(const NodeID &node, const int input_socket_index) const
  {
    return std::to_string(node * 1000 + input_socket_index);
  }

  std::string output_socket_debug_name(const NodeID &node, const int output_socket_index) const
  {
    return std::to_string(node * 1000 + output_socket_index);
  }
};

};  // namespace blender::fn::node_graph_example

namespace blender::fn {

template<typename NodeGraph> inline std::string node_graph_to_dot(const NodeGraph &graph)
{
  using NodeID = typename NodeGraph::NodeID;

  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<NodeID, dot::NodeWithSocketsRef> dot_nodes;

  graph.foreach_node([&](const NodeID &node) {
    dot::Node &dot_node = digraph.new_node("");
    Vector<std::string> input_names;
    Vector<std::string> output_names;
    for (const int i : IndexRange(graph.node_inputs_size(node))) {
      input_names.append(graph.input_socket_debug_name(node, i));
    }
    for (const int i : IndexRange(graph.node_outputs_size(node))) {
      output_names.append(graph.output_socket_debug_name(node, i));
    }
    dot_nodes.add_new(
        node,
        dot::NodeWithSocketsRef(dot_node, graph.node_debug_name(node), input_names, output_names));
  });

  graph.foreach_link([&](const NodeID &from_node,
                         const int from_index,
                         const NodeID &to_node,
                         const int to_index) {
    dot::NodeWithSocketsRef &from_dot_node = dot_nodes.lookup(from_node);
    dot::NodeWithSocketsRef &to_dot_node = dot_nodes.lookup(to_node);
    digraph.new_edge(from_dot_node.output(from_index), to_dot_node.input(to_index));
  });

  return digraph.to_dot_string();
}

}  // namespace blender::fn
