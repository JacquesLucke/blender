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

#include "BLI_map.hh"
#include "BLI_vector.hh"

namespace blender::fn::sgraph {

class SimpleSGraph {
 public:
  struct NodeInfo {
    std::string name;
    int inputs_size = 0;
    int outputs_size = 0;

    void set_min_inputs_size(int size)
    {
      this->inputs_size = std::max(this->inputs_size, size);
    }

    void set_min_outputs_size(int size)
    {
      this->outputs_size = std::max(this->outputs_size, size);
    }
  };

  struct LinkInfo {
    std::string from_node;
    int from_index;
    std::string to_node;
    int to_index;
  };

  Map<std::string, NodeInfo> nodes;
  Vector<LinkInfo> links;

  void add_node(std::string name)
  {
    this->nodes.add(name, {});
  }

  void add_link(std::string from_node, int from_index, std::string to_node, int to_index)
  {
    this->nodes.lookup_or_add_default(from_node).set_min_outputs_size(from_index + 1);
    this->nodes.lookup_or_add_default(to_node).set_min_inputs_size(to_index + 1);
    this->links.append({from_node, from_index, to_node, to_index});
  }
};

class SimpleSGraphAdapter {
 public:
  using NodeID = std::string;

 private:
  SimpleSGraph &graph_;

 public:
  SimpleSGraphAdapter(SimpleSGraph &graph) : graph_(graph)
  {
  }

  int node_inputs_size(const NodeID &node) const
  {
    return graph_.nodes.lookup(node).inputs_size;
  }

  int node_outputs_size(const NodeID &node) const
  {
    return graph_.nodes.lookup(node).outputs_size;
  }

  template<typename F> void foreach_node(const F &f) const
  {
    for (const NodeID &node : graph_.nodes.keys()) {
      f(node);
    }
  }

  template<typename F>
  void foreach_linked_input(const NodeID &node, const int output_socket_index, const F &f) const
  {
    for (const SimpleSGraph::LinkInfo &link : graph_.links) {
      if (link.from_node == node && link.from_index == output_socket_index) {
        f(link.to_node, link.to_index);
      }
    }
  }

  template<typename F>
  void foreach_linked_output(const NodeID &node, const int input_socket_index, const F &f) const
  {
    for (const SimpleSGraph::LinkInfo &link : graph_.links) {
      if (link.to_node == node && link.to_index == input_socket_index) {
        f(link.from_node, link.from_index);
      }
    }
  }

  std::string node_debug_name(const NodeID &node) const
  {
    return node;
  }

  std::string input_socket_debug_name(const NodeID &node, const int input_socket_index) const
  {
    std::stringstream ss;
    ss << node << ":in:" << input_socket_index;
    return ss.str();
  }

  std::string output_socket_debug_name(const NodeID &node, const int output_socket_index) const
  {
    std::stringstream ss;
    ss << node << ":out:" << output_socket_index;
    return ss.str();
  }
};

}  // namespace blender::fn::sgraph
