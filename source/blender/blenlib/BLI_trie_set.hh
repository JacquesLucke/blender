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

#include "BLI_dot_export.hh"
#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"

namespace blender {

struct TrieSetNode {
  Map<uint8_t, std::unique_ptr<TrieSetNode>> children;
  bool is_terminal = false;

  dot::Node &add_to_dot_graph(dot::DirectedGraph &graph) const
  {
    dot::Node &node = graph.new_node("");
    if (is_terminal) {
      node.set_background_color("#AAEEAA");
    }
    for (auto &&[value, trie_node] : children.items()) {
      dot::Node &child_node = trie_node->add_to_dot_graph(graph);
      dot::DirectedEdge &edge = graph.new_edge(node, child_node);
      edge.set_label(std::to_string(value) + " (" + (char)value + ")");
    }
    return node;
  }
};

class TrieSet {
 private:
  TrieSetNode root_;
  int64_t size_ = 0;

 public:
  int64_t size() const
  {
    return size_;
  }

  bool add(StringRef str)
  {
    return this->add(Span<uint8_t>((const uint8_t *)str.data(), str.size()));
  }
  bool add(Span<uint8_t> data)
  {
    Span<uint8_t> remaining_data = data;
    TrieSetNode *current = &root_;

    while (!remaining_data.is_empty()) {
      current = current->children
                    .lookup_or_add_cb(remaining_data[0],
                                      []() { return std::make_unique<TrieSetNode>(); })
                    .get();
      remaining_data = remaining_data.drop_front(1);
    }

    const bool was_terminal = current->is_terminal;
    current->is_terminal = true;
    const bool is_newly_added = !was_terminal;
    size_ += is_newly_added;
    return is_newly_added;
  }

  bool has_prefix_of(StringRef str) const
  {
    return this->has_prefix_of(Span<uint8_t>((const uint8_t *)str.data(), str.size()));
  }
  bool has_prefix_of(Span<uint8_t> data) const
  {
    Span<uint8_t> remaining_data = data;
    const TrieSetNode *current = &root_;
    while (!remaining_data.is_empty()) {
      if (current->is_terminal) {
        return true;
      }
      const std::unique_ptr<TrieSetNode> *child = current->children.lookup_ptr(remaining_data[0]);
      if (child == nullptr) {
        return false;
      }
      current = child->get();
      remaining_data = remaining_data.drop_front(1);
    }
    return current->is_terminal;
  }

  std::string to_dot() const
  {
    dot::DirectedGraph graph;
    root_.add_to_dot_graph(graph);
    return graph.to_dot_string();
  }
};

}  // namespace blender
