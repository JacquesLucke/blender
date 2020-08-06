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
  Array<uint8_t> values;
  Map<uint8_t, std::unique_ptr<TrieSetNode>> children;
  bool is_terminal = false;

  dot::Node &add_to_dot_graph(dot::DirectedGraph &graph) const
  {
    std::string name;
    for (uint8_t value : values) {
      name += (char)value;
    }
    dot::Node &node = graph.new_node(name);
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

template<typename T> static int64_t get_common_prefix_length(Span<T> values1, Span<T> values2)
{
  int64_t count = 0;
  while (true) {
    if (count >= values1.size()) {
      break;
    }
    if (count >= values2.size()) {
      break;
    }
    if (values1[count] != values2[count]) {
      break;
    }
    count++;
  }
  return count;
}

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

    while (true) {
      if (remaining_data.is_empty()) {
        const bool was_terminal = current->is_terminal;
        current->is_terminal = true;
        const bool is_newly_added = !was_terminal;
        size_ += is_newly_added;
        return is_newly_added;
      }

      uint8_t first_value = remaining_data[0];
      remaining_data = remaining_data.drop_front(1);

      if (current->children.contains(first_value)) {
        std::unique_ptr<TrieSetNode> &child = current->children.lookup(first_value);
        TrieSetNode *child_ptr = child.get();
        int64_t common_prefix_length = get_common_prefix_length<uint8_t>(child->values,
                                                                         remaining_data);
        if (common_prefix_length == child->values.size()) {
          current = child.get();
          remaining_data = remaining_data.drop_front(common_prefix_length);
        }
        else {
          auto intermediate_node = std::make_unique<TrieSetNode>();
          intermediate_node->values = remaining_data.take_front(common_prefix_length);
          intermediate_node->children.add_new(child->values[common_prefix_length],
                                              std::move(child));

          Array<uint8_t> copied_values = child_ptr->values.as_span().drop_front(
              common_prefix_length + 1);
          child_ptr->values = std::move(copied_values);

          if (common_prefix_length < remaining_data.size()) {
            remaining_data = remaining_data.drop_front(common_prefix_length);
            auto new_node = std::make_unique<TrieSetNode>();
            new_node->is_terminal = true;
            new_node->values = remaining_data.drop_front(1);
            intermediate_node->children.add_new(remaining_data[0], std::move(new_node));
          }
          else {
            intermediate_node->is_terminal = true;
          }
          child = std::move(intermediate_node);
          return true;
        }
      }
      else {
        auto new_node = std::make_unique<TrieSetNode>();
        new_node->is_terminal = true;
        new_node->values = remaining_data;
        current->children.add_new(first_value, std::move(new_node));
        return true;
      }
    }
  }

  bool has_prefix_of(StringRef str) const
  {
    return this->has_prefix_of(Span<uint8_t>((const uint8_t *)str.data(), str.size()));
  }
  bool has_prefix_of(Span<uint8_t> data) const
  {
    Span<uint8_t> remaining_data = data;
    const TrieSetNode *current = &root_;
    while (true) {
      if (current->is_terminal) {
        return true;
      }
      if (remaining_data.is_empty()) {
        return false;
      }

      uint8_t first_value = remaining_data[0];
      remaining_data = remaining_data.drop_front(1);
      const std::unique_ptr<TrieSetNode> *child = current->children.lookup_ptr(first_value);
      if (child == nullptr) {
        return false;
      }

      int64_t common_prefix_length = get_common_prefix_length<uint8_t>(remaining_data,
                                                                       (*child)->values);
      if (common_prefix_length == (*child)->values.size()) {
        current = child->get();
      }
      else {
        return false;
      }
    }
  }

  std::string to_dot() const
  {
    dot::DirectedGraph graph;
    root_.add_to_dot_graph(graph);
    return graph.to_dot_string();
  }
};

}  // namespace blender
