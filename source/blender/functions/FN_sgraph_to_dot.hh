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
// #include "FN_sgraph_validate_adapter.hh"

#include "BLI_dot_export.hh"

namespace blender::fn::sgraph {

template<typename SGraphAccessor>
inline std::string sgraph_to_dot(const SGraphT<SGraphAccessor> &graph)
{
  using SGraph = SGraphT<SGraphAccessor>;
  using Node = typename SGraph::Node;
  using Link = typename SGraph::Link;

  // sgraph_adapter_is_valid(graph.adapter());

  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<Node, dot::NodeWithSocketsRef> dot_nodes;

  graph.foreach_node([&](const Node &node) {
    dot::Node &dot_node = digraph.new_node("");
    Vector<std::string> input_names;
    Vector<std::string> output_names;
    for (const int i : graph.node_inputs_range(node)) {
      input_names.append(graph.input_debug_name(graph.node_input(node, i)));
    }
    for (const int i : graph.node_outputs_range(node)) {
      output_names.append(graph.output_debug_name(graph.node_output(node, i)));
    }
    dot_nodes.add_new(
        node,
        dot::NodeWithSocketsRef(dot_node, graph.node_debug_name(node), input_names, output_names));
  });

  graph.foreach_link([&](const Link &link) {
    dot::NodeWithSocketsRef &from_dot_node = dot_nodes.lookup(graph.link_from_node(link));
    dot::NodeWithSocketsRef &to_dot_node = dot_nodes.lookup(graph.link_to_node(link));
    digraph.new_edge(from_dot_node.output(graph.index_of_output(graph.link_from_socket(link))),
                     to_dot_node.input(graph.index_of_input(graph.link_to_socket(link))));
  });

  return digraph.to_dot_string();
}

}  // namespace blender::fn::sgraph
