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

#include "BLI_array.hh"
#include "BLI_dot_export.hh"

namespace blender {

template<typename DataArray> class InplacePriorityQueue {
 private:
  DataArray &data_;
  Array<int64_t> index_map_;
  int64_t heap_size_ = 0;

 public:
  InplacePriorityQueue(DataArray &data) : data_(data), index_map_(data_.size())
  {
    for (const int64_t i : index_map_.index_range()) {
      index_map_[i] = i;
    }
  }

  void build()
  {
    const int final_heap_size = this->get_data_size();
    if (final_heap_size > 0) {
      for (int64_t i = final_heap_size / 2 - 1; i--;) {
        this->heapify(i, final_heap_size);
      }
    }
    heap_size_ = final_heap_size;
  }

  std::string all_to_dot() const
  {
    return this->partial_to_dot(this->get_data_size());
  }

  std::string partial_to_dot(const int size) const
  {
    dot::DirectedGraph digraph;
    Array<dot::Node *> dot_nodes(size);
    for (const int i : IndexRange(size)) {
      const std::string name = this->index_to_string(i);
      dot::Node &node = digraph.new_node(name);
      node.set_shape(dot::Attr_shape::Rectangle);
      node.attributes.set("ordering", "out");
      dot_nodes[i] = &node;
      if (i > 0) {
        const int64_t parent = this->get_parent(i);
        digraph.new_edge(*dot_nodes[parent], node);
      }
    }
    return digraph.to_dot_string();
  }

 private:
  int64_t get_data_size() const
  {
    return data_.size();
  }

  std::string index_to_string(const int64_t index) const
  {
    return data_.index_to_string(index);
  }

  void swap_indices(const int64_t a, const int64_t b)
  {
    data_.swap_indices(a, b);
    std::swap(index_map_[a], index_map_[b]);
  }

  bool is_higher_priority(const int64_t a, const int64_t b)
  {
    return data_.is_higher_priority(a, b);
  }

  void heapify(const int64_t index, const int64_t heap_size)
  {
    int64_t min_index = index;
    const int left = this->get_left(index);
    const int right = this->get_right(index);
    if (left < heap_size && this->is_higher_priority(left, min_index)) {
      min_index = left;
    }
    if (right < heap_size && this->is_higher_priority(right, min_index)) {
      min_index = right;
    }
    if (min_index != index) {
      this->swap_indices(index, min_index);
      this->heapify(min_index, heap_size);
    }
  }

  int64_t get_parent(const int64_t child) const
  {
    BLI_assert(child > 0);
    return (child - 1) / 2;
  }

  int64_t get_left(const int64_t parent) const
  {
    return parent * 2 + 1;
  }

  int64_t get_right(const int64_t parent) const
  {
    return parent * 2 + 2;
  }
};

}  // namespace blender
