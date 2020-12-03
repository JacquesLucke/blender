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
#include "BLI_rand.hh"
#include "BLI_stack.hh"
#include "BLI_utildefines.h"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender::kdtree {

enum class NodeType {
  Inner,
  Leaf,
};

struct Node {
  NodeType type;

  Node(NodeType type) : type(type)
  {
  }
};

template<typename Point> struct LeafNode_ : public Node {
  MutableSpan<Point> points;

  LeafNode_() : Node(NodeType::Leaf)
  {
  }
};

struct InnerNode : public Node {
  int dim;
  float value;
  Node *children[2];

  InnerNode() : Node(NodeType::Inner)
  {
  }
};

template<typename Point> struct DefaultPointAdapter {
};

template<typename Point,
         int MaxLeafSize = 8,
         typename PointAdapater = typename DefaultPointAdapter<Point>::type>
class KDTree : NonCopyable, NonMovable {
 private:
  using LeafNode = LeafNode_<Point>;
  static constexpr int DIM = PointAdapater::DIM;
  static constexpr int MAX_LEAF_SIZE = MaxLeafSize;

  Node *root_ = nullptr;
  PointAdapater adapter_;

 public:
  KDTree(MutableSpan<Point> points, PointAdapater adapter = {}) : adapter_(adapter)
  {
    root_ = this->build_tree(points);
  }

  ~KDTree()
  {
    if (root_ != nullptr) {
      this->free_tree(root_);
    }
  }

  const Point *find_nearest(const float *co) const
  {
    const Point *best_point = nullptr;
    this->foreach_in_shrinking_radius(
        co, [&](const Point &point, const float distance_sq, float *r_max_distance_sq) {
          best_point = &point;
          *r_max_distance_sq = distance_sq;
        });
    return best_point;
  }

  const Point *find_nearest(const Point &point) const
  {
    std::array<float, DIM> co = point_as_float_array(point);
    return this->find_nearest(co.data());
  }

  template<typename Func>
  void foreach_in_radius(const float *co, const float radius, const Func &func) const
  {
    this->foreach_in_shrinking_radius(
        co,
        [&](const Point &point, const float distance_sq, float *UNUSED(r_max_distance_sq)) {
          func(point, distance_sq);
        },
        radius);
  }

  template<typename Func>
  void foreach_in_radius(const Point &point, const float radius, const Func &func) const
  {
    std::array<float, DIM> co = point_as_float_array(point);
    this->foreach_in_radius(co.data(), radius, func);
  }

  template<typename Func>
  void foreach_in_shrinking_radius(
      const float *co,
      const Func &func,
      const float radius = std::numeric_limits<float>::infinity()) const
  {
    float max_distance_sq = radius * radius;
    this->foreach_in_shrinking_radius_internal(*root_, co, func, &max_distance_sq);
  }

  template<typename Func>
  void foreach_in_shrinking_radius(const Point &point, const Func &func, const float radius) const
  {
    std::array<float, DIM> co = point_as_float_array(point);
    this->foreach_in_shrinking_radius(co.data(), func, radius);
  }

  std::string to_dot() const
  {
    dot::DirectedGraph digraph;
    digraph.set_rankdir(dot::Attr_rankdir::TopToBottom);
    this->make_dot_nodes(digraph, *root_);
    return digraph.to_dot_string();
  }

  void print_stats() const
  {
    Vector<Vector<const Node *>> nodes_by_level;
    nodes_by_level.append({root_});
    while (!nodes_by_level.last().is_empty()) {
      Vector<const Node *> children;
      for (const Node *node : nodes_by_level.last()) {
        if (node->type == NodeType::Inner) {
          const InnerNode *inner_node = static_cast<const InnerNode *>(node);
          children.append(inner_node->children[0]);
          children.append(inner_node->children[1]);
        }
      }
      nodes_by_level.append(std::move(children));
    }

    for (const int level : nodes_by_level.index_range()) {
      Span<const Node *> nodes = nodes_by_level[level];
      std::cout << "Level: " << level << "\t Nodes: " << nodes.size() << "\n";
    }
  }

 private:
  BLI_NOINLINE Node *build_tree(MutableSpan<Point> points)
  {
    if (points.size() <= MAX_LEAF_SIZE) {
      LeafNode *node = new LeafNode();
      node->points = points;
      return node;
    }

    InnerNode *node = new InnerNode();
    this->find_splitter(points, &node->dim, &node->value);

    MutableSpan<Point> left_points, right_points;
    this->split_points(points, node->dim, node->value, &left_points, &right_points);

    node->children[0] = this->build_tree(left_points);
    node->children[1] = this->build_tree(right_points);
    return node;
  }

  BLI_NOINLINE void find_splitter(MutableSpan<Point> points,
                                  int *r_split_dim,
                                  float *r_split_value) const
  {
    if (points.size() < 50) {
      this->find_splitter_exact(points, r_split_dim, r_split_value);
      return;
    }

    const int sample_size = std::max<int>(20, points.size() / 100);
    RandomNumberGenerator rng;
    Array<Point> point_samples(sample_size);
    for (const int i : IndexRange(sample_size)) {
      point_samples[i] = points[rng.get_int32(points.size())];
    }
    this->find_splitter_exact(point_samples, r_split_dim, r_split_value);
  }

  BLI_NOINLINE void find_splitter_exact(MutableSpan<Point> points,
                                        int *r_split_dim,
                                        float *r_split_value) const
  {
    int best_dim = -1;
    float highest_deviation = -1.0f;
    for (const int dim : IndexRange(DIM)) {
      float total_sum = 0.0f;
      for (const Point &point : points) {
        total_sum += adapter_.get(point, dim);
      }
      const float average = total_sum / points.size();
      float deviation = 0.0f;
      for (const Point &point : points) {
        float distance_to_average = adapter_.get(point, dim) - average;
        deviation += distance_to_average * distance_to_average;
      }

      if (deviation > highest_deviation) {
        best_dim = dim;
        highest_deviation = deviation;
      }
    }

    const int median_position = points.size() / 2;
    std::nth_element(points.begin(),
                     points.begin() + median_position,
                     points.end(),
                     [&](const Point &a, const Point &b) {
                       return adapter_.get(a, best_dim) < adapter_.get(b, best_dim);
                     });
    *r_split_dim = best_dim;
    *r_split_value = adapter_.get(points[median_position], best_dim);
  }

  BLI_NOINLINE void split_points(MutableSpan<Point> points,
                                 const int split_dim,
                                 const float split_value,
                                 MutableSpan<Point> *r_left_points,
                                 MutableSpan<Point> *r_right_points)
  {
    auto is_larger_than_split_value = [&](const Point &p) {
      return adapter_.get(p, split_dim) > split_value;
    };

    int i = 0;
    int j = points.size() - 1;
    while (i < j) {
      if (is_larger_than_split_value(points[i])) {
        std::swap(points[i], points[j]);
        j--;
      }
      else {
        i++;
      }
    }

    /* Move split closer to center, when there are multiple points with the same value. Points with
     * the same value might go into different subtrees.
     * TODO: This does not work perfectly yet, because the points with the split value might not be
     * all around the median. It might be possible to keep track of these points in a vector and
     * copy them to the middle afterwards. */
    int split_pos = i;
    const int median_pos = points.size() / 2;
    while (split_pos < median_pos && split_pos + 1 < points.size()) {
      if (adapter_.get(points[split_pos], split_dim) ==
          adapter_.get(points[split_pos + 1], split_dim)) {
        split_pos++;
      }
      else {
        break;
      }
    }
    while (split_pos > median_pos && split_pos > 0) {
      if (adapter_.get(points[split_pos], split_dim) ==
          adapter_.get(points[split_pos - 1], split_dim)) {
        split_pos--;
      }
      else {
        break;
      }
    }

    *r_left_points = points.slice(0, split_pos);
    *r_right_points = points.slice(split_pos, points.size() - split_pos);
  }

  BLI_NOINLINE void free_tree(Node *node)
  {
    if (node->type == NodeType::Inner) {
      InnerNode *inner_node = static_cast<InnerNode *>(node);
      this->free_tree(inner_node->children[0]);
      this->free_tree(inner_node->children[1]);
      delete inner_node;
    }
    else {
      delete static_cast<LeafNode *>(node);
    }
  }

  template<typename Func>
  BLI_NOINLINE void foreach_in_shrinking_radius_internal(const Node &root,
                                                         const float *co,
                                                         const Func &func,
                                                         float *r_max_distance_sq) const
  {
    Stack<const Node *, 30> stack;
    stack.push(&root);
    bool is_going_down = true;

    while (!stack.is_empty()) {
      const Node &node = *stack.peek();
      if (node.type == NodeType::Inner) {
        const InnerNode &inner_node = static_cast<const InnerNode &>(node);
        const float co_in_dim = co[inner_node.dim];
        const float signed_split_distance = co_in_dim - inner_node.value;
        const int initial_child = signed_split_distance > 0.0f;
        if (is_going_down) {
          stack.push(inner_node.children[initial_child]);
        }
        else {
          const float split_distance_sq = signed_split_distance * signed_split_distance;
          if (split_distance_sq < *r_max_distance_sq) {
            const int other_child = 1 - initial_child;
            stack.peek() = inner_node.children[other_child];
            is_going_down = true;
          }
          else {
            stack.pop();
          }
        }
      }
      else {
        const LeafNode &leaf_node = static_cast<const LeafNode &>(node);
        for (const Point &point : leaf_node.points) {
          const float distance_sq = this->calc_distance_sq(co, point);
          if (distance_sq <= *r_max_distance_sq) {
            float new_max_distance_sq = *r_max_distance_sq;
            func(point, distance_sq, &new_max_distance_sq);
            BLI_assert(new_max_distance_sq <= *r_max_distance_sq);
            *r_max_distance_sq = new_max_distance_sq;
          }
        }
        stack.pop();
        is_going_down = false;
      }
    }
  }

  float calc_distance_sq(const float *co, const Point &point) const
  {
    float sum = 0.0f;
    for (const int dim : IndexRange(DIM)) {
      const float value = co[dim] - adapter_.get(point, dim);
      sum += value * value;
    }
    return sum;
  }

  std::array<float, DIM> point_as_float_array(const Point &point) const
  {
    std::array<float, DIM> co;
    for (const int dim : IndexRange(DIM)) {
      co[dim] = adapter_.get(point, dim);
    }
    return co;
  }

  dot::Node &make_dot_nodes(dot::DirectedGraph &digraph, const Node &node) const
  {
    if (node.type == NodeType::Inner) {
      return this->make_dot_nodes(digraph, static_cast<const InnerNode &>(node));
    }
    return this->make_dot_nodes(digraph, static_cast<const LeafNode &>(node));
  }

  dot::Node &make_dot_nodes(dot::DirectedGraph &digraph, const InnerNode &node) const
  {
    const std::string name = "[" + std::to_string(node.dim) + "] = " + std::to_string(node.value);
    dot::Node &dot_node = digraph.new_node(name);
    dot_node.set_shape(dot::Attr_shape::Rectangle);
    dot_node.attributes.set("ordering", "out");
    dot::Node &child0 = this->make_dot_nodes(digraph, *node.children[0]);
    dot::Node &child1 = this->make_dot_nodes(digraph, *node.children[1]);
    digraph.new_edge(dot_node, child0);
    digraph.new_edge(dot_node, child1);
    return dot_node;
  }

  dot::Node &make_dot_nodes(dot::DirectedGraph &digraph, const LeafNode &node) const
  {
    std::stringstream ss;
    for (const Point &point : node.points) {
      ss << "(";
      for (const int dim : IndexRange(DIM)) {
        ss << adapter_.get(point, dim);
        if (dim < DIM - 1) {
          ss << ", ";
        }
      }
      ss << "), ";
    }
    dot::Node &dot_node = digraph.new_node(ss.str());
    return dot_node;
  }
};

}  // namespace blender::kdtree
