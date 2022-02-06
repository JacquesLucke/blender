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

#include "FN_sgraph.hh"

#include "BLI_dot_export.hh"

namespace blender::fn::sgraph {

template<typename SGraphAdapter>
inline std::string sgraph_to_dot(const SGraphT<SGraphAdapter> &graph)
{
  using SGraph = SGraphT<SGraphAdapter>;
  using Node = typename SGraph::Node;
  using Link = typename SGraph::Link;

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
