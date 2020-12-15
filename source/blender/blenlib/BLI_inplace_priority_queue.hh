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
 * This data structure can add a priority queue on top of any array.
 * The priority queue does not change or reorder values in the underlying array.
 * Instead, it only maintains indices into the array.
 */
template<typename T, typename FirstHasHigherPriority = std::greater<T>>
class InplacePriorityQueue {
 private:
  Span<T> data_;
  Array<int64_t> heap_to_orig_;
  Array<int64_t> orig_to_heap_;
  int64_t heap_size_ = 0;
  FirstHasHigherPriority first_has_higher_priority_fn_;

 public:
  InplacePriorityQueue(Span<T> data)
      : data_(data), heap_to_orig_(data_.size()), orig_to_heap_(data_.size())
  {
    for (const int64_t i : IndexRange(data_.size())) {
      heap_to_orig_[i] = i;
      orig_to_heap_[i] = i;
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
    return heap_to_orig_[0];
  }

  int64_t pop_top()
  {
    BLI_assert(!this->is_empty());
    const int64_t top_index_orig = heap_to_orig_[0];
    heap_size_--;
    if (heap_size_ > 1) {
      this->swap_indices(0, heap_size_);
      this->heapify(0, heap_size_);
    }
    return top_index_orig;
  }

  void priority_decreased(const int64_t index)
  {
    const int64_t heap_index = orig_to_heap_[index];
    BLI_assert(heap_index < heap_size_);
    this->heapify(heap_index, heap_size_);
  }

  void priority_increased(const int64_t index)
  {
    int64_t current = orig_to_heap_[index];
    BLI_assert(current < heap_size_);
    while (true) {
      if (current == 0) {
        break;
      }
      const int64_t parent = this->get_parent(current);
      if (this->first_has_higher_priority(parent, current)) {
        break;
      }
      this->swap_indices(current, parent);
      current = parent;
    }
  }

  void priority_changed(const int64_t index)
  {
    this->priority_increased(index);
    this->priority_decreased(index);
  }

  std::string all_to_dot() const
  {
    return this->partial_to_dot(data_.size());
  }

  std::string active_to_dot() const
  {
    return this->partial_to_dot(heap_size_);
  }

 private:
  bool first_has_higher_priority(const int64_t a, const int64_t b)
  {
    const T &value_a = data_[heap_to_orig_[a]];
    const T &value_b = data_[heap_to_orig_[b]];
    return first_has_higher_priority_fn_(value_a, value_b);
  }

  void swap_indices(const int64_t a, const int64_t b)
  {
    std::swap(heap_to_orig_[a], heap_to_orig_[b]);
    orig_to_heap_[heap_to_orig_[a]] = a;
    orig_to_heap_[heap_to_orig_[b]] = b;
  }

  void heapify(const int64_t index, const int64_t heap_size)
  {
    int64_t min_index = index;
    const int left = this->get_left(index);
    const int right = this->get_right(index);
    if (left < heap_size && this->first_has_higher_priority(left, min_index)) {
      min_index = left;
    }
    if (right < heap_size && this->first_has_higher_priority(right, min_index)) {
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

  std::string partial_to_dot(const int size) const
  {
    dot::DirectedGraph digraph;
    Array<dot::Node *> dot_nodes(size);
    for (const int i : IndexRange(size)) {
      std::stringstream ss;
      ss << data_[heap_to_orig_[i]];
      const std::string name = ss.str();
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
};

}  // namespace blender
