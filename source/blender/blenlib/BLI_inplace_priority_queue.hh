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

/**
 * This data structure can add a priority queue on top of any array. Elements in the array are not
 * reordered. Instead this priority queue just maintains indices into the array.
 */
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
    const int final_heap_size = data_.size();
    if (final_heap_size > 0) {
      for (int64_t i = final_heap_size / 2 - 1; i--;) {
        this->heapify(i, final_heap_size);
      }
    }
    heap_size_ = final_heap_size;
  }

  int64_t size() const
  {
    return heap_size_;
  }

  bool is_empty() const
  {
    return heap_size_ == 0;
  }

  int64_t peek_top() const
  {
    BLI_assert(!this->is_empty());
    return index_map_[0];
  }

  int64_t pop_top()
  {
    BLI_assert(!this->is_empty());
    const int64_t top_index = index_map_[0];
    heap_size_--;
    if (heap_size_ > 1) {
      this->swap_indices(0, heap_size_);
      this->heapify(0, heap_size_);
    }
    return top_index;
  }

  std::string all_to_dot() const
  {
    return this->partial_to_dot(data_.size());
  }

  std::string partial_to_dot(const int size) const
  {
    dot::DirectedGraph digraph;
    Array<dot::Node *> dot_nodes(size);
    for (const int i : IndexRange(size)) {
      const std::string name = this->data_.get_value_string(index_map_[i]);
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

  DataArray &data()
  {
    return data_;
  }

 private:
  bool is_higher_priority(const int64_t a, const int64_t b)
  {
    return data_.is_higher_priority(index_map_[a], index_map_[b]);
  }

  void swap_indices(const int64_t a, const int64_t b)
  {
    std::swap(index_map_[a], index_map_[b]);
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
