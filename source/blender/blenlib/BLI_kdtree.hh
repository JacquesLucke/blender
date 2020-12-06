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
#include "BLI_timeit.hh"
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
  Node *parent = nullptr;

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
  std::array<Node *, 2> children;
  std::array<std::array<const void *, 2>, 2> prefetch_pointers = {0};

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

  PointAdapater adapter_;
  Array<Point> points_;
  Node *root_ = nullptr;

 public:
  KDTree(Span<Point> points, PointAdapater adapter = {}) : adapter_(adapter), points_(points)
  {
    root_ = this->build_tree(points_);
    this->set_parent_and_prefetch_pointers(*root_);
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

  void print_tree_correctness_errors() const
  {
    this->foreach_inner_node(*root_, [&](const InnerNode &inner_node) {
      this->foreach_point(*inner_node.children[0], [&](const Point &point) {
        if (adapter_.get(point, inner_node.dim) > inner_node.value) {
          std::cout << "error: " << point << "\n";
        }
      });
      this->foreach_point(*inner_node.children[1], [&](const Point &point) {
        if (adapter_.get(point, inner_node.dim) < inner_node.value) {
          std::cout << "error: " << point << "\n";
        }
      });
    });

    this->foreach_inner_node(*root_, [&](const InnerNode &inner_node) {
      if (inner_node.children[0]->parent != &inner_node) {
        std::cout << "wrong parent\n";
      }
      if (inner_node.children[1]->parent != &inner_node) {
        std::cout << "wrong parent\n";
      }
    });

    int point_count = 0;
    this->foreach_leaf_node(
        *root_, [&](const LeafNode &leaf_node) { point_count += leaf_node.points.size(); });
    if (point_count != points_.size()) {
      std::cout << "incorrect number of points\n";
    }
  }

 private:
  BLI_NOINLINE Node *build_tree(MutableSpan<Point> points)
  {
    if (points.size() <= MAX_LEAF_SIZE) {
      return this->build_tree__leaf(points);
    }
    if (points.size() >= 10'000) {
      return this->build_tree__three_levels(points);
    }
    return this->build_tree__single_level(points);
  }

  BLI_NOINLINE Node *build_tree__leaf(MutableSpan<Point> points)
  {
    LeafNode *node = new LeafNode();
    node->points = points;
    return node;
  }

  BLI_NOINLINE Node *build_tree__single_level(MutableSpan<Point> points)
  {
    InnerNode *node = new InnerNode();
    this->find_splitter_approximate(points, &node->dim, &node->value);

    MutableSpan<Point> left_points, right_points;
    this->split_points(points, node->dim, node->value, &left_points, &right_points);

    node->children[0] = this->build_tree(left_points);
    node->children[1] = this->build_tree(right_points);

    return node;
  }

  BLI_NOINLINE Node *build_tree__three_levels(MutableSpan<Point> points)
  {
    InnerNode *inner1 = new InnerNode();
    std::array<InnerNode *, 2> inner2 = {new InnerNode(), new InnerNode()};
    std::array<std::array<InnerNode *, 2>, 2> inner3;
    inner3[0] = {new InnerNode(), new InnerNode()};
    inner3[1] = {new InnerNode(), new InnerNode()};

    inner1->children = {inner2[0], inner2[1]};
    inner2[0]->children = {inner3[0][0], inner3[0][1]};
    inner2[1]->children = {inner3[1][0], inner3[1][1]};

    const int sample_size = std::max<int>(100, points.size() / 100);
    Array<Point> point_samples = this->get_random_samples(points, sample_size);
    this->find_splitter_exact(point_samples, &inner1->dim, &inner1->value);

    MutableSpan<Point> split_points1[2];
    this->split_points(
        point_samples, inner1->dim, inner1->value, &split_points1[0], &split_points1[1]);

    this->find_splitter_exact(split_points1[0], &inner2[0]->dim, &inner2[0]->value);
    this->find_splitter_exact(split_points1[1], &inner2[1]->dim, &inner2[1]->value);

    MutableSpan<Point> split_points2[2][2];
    this->split_points(split_points1[0],
                       inner2[0]->dim,
                       inner2[0]->value,
                       &split_points2[0][0],
                       &split_points2[0][1]);
    this->split_points(split_points1[1],
                       inner2[1]->dim,
                       inner2[1]->value,
                       &split_points2[1][0],
                       &split_points2[1][1]);

    this->find_splitter_exact(split_points2[0][0], &inner3[0][0]->dim, &inner3[0][0]->value);
    this->find_splitter_exact(split_points2[0][1], &inner3[0][1]->dim, &inner3[0][1]->value);
    this->find_splitter_exact(split_points2[1][0], &inner3[1][0]->dim, &inner3[1][0]->value);
    this->find_splitter_exact(split_points2[1][1], &inner3[1][1]->dim, &inner3[1][1]->value);

    const int split_dim2[2] = {inner2[0]->dim, inner2[1]->dim};
    const float split_value2[2] = {inner2[0]->value, inner2[1]->value};
    const int split_dim3[2][2] = {{inner3[0][0]->dim, inner3[0][1]->dim},
                                  {inner3[1][0]->dim, inner3[1][1]->dim}};
    const float split_value3[2][2] = {{inner3[0][0]->value, inner3[0][1]->value},
                                      {inner3[1][0]->value, inner3[1][1]->value}};

    Vector<Point> point_buckets[2][2][2];
    this->split_points_three_times(points,
                                   inner1->dim,
                                   inner1->value,
                                   split_dim2,
                                   split_value2,
                                   split_dim3,
                                   split_value3,
                                   point_buckets);

    int offset = 0;
    for (const int i : IndexRange(2)) {
      for (const int j : IndexRange(2)) {
        for (const int k : IndexRange(2)) {
          Vector<Point> &bucket = point_buckets[i][j][k];
          initialized_copy_n(bucket.data(), bucket.size(), points.data() + offset);
          inner3[i][j]->children[k] = this->build_tree(points.slice(offset, bucket.size()));
          offset += bucket.size();
        }
      }
    }

    return inner1;
  }

  BLI_NOINLINE void find_splitter_approximate(MutableSpan<Point> points,
                                              int *r_split_dim,
                                              float *r_split_value) const
  {
    if (points.size() < 50) {
      this->find_splitter_exact(points, r_split_dim, r_split_value);
      return;
    }

    const int sample_size = std::max<int>(20, points.size() / 100);
    Array<Point> point_samples = this->get_random_samples(points, sample_size);
    this->find_splitter_exact(point_samples, r_split_dim, r_split_value);
  }

  BLI_NOINLINE Array<Point> get_random_samples(Span<Point> points, const int amount) const
  {
    RandomNumberGenerator rng;
    Array<Point> point_samples(amount);
    for (const int i : IndexRange(amount)) {
      point_samples[i] = points[rng.get_int32(points.size())];
    }
    return point_samples;
  }

  BLI_NOINLINE void find_splitter_exact(MutableSpan<Point> points,
                                        int *r_split_dim,
                                        float *r_split_value) const
  {
    int best_dim = this->find_best_split_dim(points);
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

  BLI_NOINLINE int find_best_split_dim(Span<Point> points) const
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
    return best_dim;
  }

  BLI_NOINLINE void split_points_three_times(Span<Point> points,
                                             const int split_dim1,
                                             const float split_value1,
                                             const int split_dim2[2],
                                             const float split_value2[2],
                                             const int split_dim3[2][2],
                                             const float split_value3[2][2],
                                             Vector<Point> r_buckets[2][2][2]) const
  {
    for (const Point &point : points) {
      const int i1 = adapter_.get(point, split_dim1) > split_value1;
      const int i2 = adapter_.get(point, split_dim2[i1]) > split_value2[i1];
      const int i3 = adapter_.get(point, split_dim3[i1][i2]) > split_value3[i1][i2];
      Vector<Point> &bucket = r_buckets[i1][i2][i3];
      bucket.append(point);
    }
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
    while (i <= j) {
      if (is_larger_than_split_value(points[i])) {
        std::swap(points[i], points[j]);
        j--;
      }
      else {
        i++;
      }
    }

    /* Move split closer to center if possible. This is necessary in some cases to ensure that the
     * input points don't end up in the same bucket, if some points are exactly on the split plane.
     */
    const int best_i = points.size() / 2;
    while (i < best_i && i < points.size()) {
      if (adapter_.get(points[i], split_dim) <= split_value) {
        i++;
      }
      else {
        break;
      }
    }
    while (i > best_i && i > 0) {
      if (adapter_.get(points[i - 1], split_dim) >= split_value) {
        i--;
      }
      else {
        break;
      }
    }

    *r_left_points = points.slice(0, i);
    *r_right_points = points.slice(i, points.size() - i);

#ifdef DEBUG
    BLI_assert(r_left_points->size() + r_right_points->size() == points.size());
    for (const Point &point : *r_left_points) {
      if (adapter_.get(point, split_dim) > split_value) {
        BLI_assert(false);
      }
    }
    for (const Point &point : *r_right_points) {
      if (adapter_.get(point, split_dim) < split_value) {
        BLI_assert(false);
      }
    }
#endif
  }

  BLI_NOINLINE void set_parent_and_prefetch_pointers(Node &node)
  {
    this->foreach_inner_node(node, [](const InnerNode &inner_node_const) {
      InnerNode &inner_node = const_cast<InnerNode &>(inner_node_const);
      for (const int i : {0, 1}) {
        Node &child = *inner_node.children[i];
        child.parent = &inner_node;
        if (child.type == NodeType::Inner) {
          InnerNode &inner_child = static_cast<InnerNode &>(child);
          inner_node.prefetch_pointers[i] = {inner_child.children[0], inner_child.children[1]};
        }
        else {
          LeafNode &leaf_child = static_cast<LeafNode &>(child);
          const void *data = leaf_child.points.data();
          inner_node.prefetch_pointers[i] = {data, POINTER_OFFSET(data, 64)};
        }
      }
    });
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

  template<typename Func> void foreach_inner_node(const Node &node, const Func &func) const
  {
    if (node.type == NodeType::Inner) {
      const InnerNode &inner_node = static_cast<const InnerNode &>(node);
      func(inner_node);
      this->foreach_inner_node(*inner_node.children[0], func);
      this->foreach_inner_node(*inner_node.children[1], func);
    }
  }

  template<typename Func> void foreach_leaf_node(const Node &node, const Func &func) const
  {
    if (node.type == NodeType::Inner) {
      const InnerNode &inner_node = static_cast<const InnerNode &>(node);
      this->foreach_leaf_node(*inner_node.children[0], func);
      this->foreach_leaf_node(*inner_node.children[1], func);
    }
    else {
      const LeafNode &leaf_node = static_cast<const LeafNode &>(node);
      func(leaf_node);
    }
  }

  template<typename Func> void foreach_point(const Node &node, const Func &func) const
  {
    this->foreach_leaf_node(node, [&](const LeafNode &leaf_node) {
      for (const Point &point : leaf_node.points) {
        func(point);
      }
    });
  }

  template<typename Func>
  BLI_NOINLINE void foreach_in_shrinking_radius_internal(const Node &root,
                                                         const float *co,
                                                         const Func &func,
                                                         float *r_max_distance_sq) const
  {
    const LeafNode &initial_leaf = this->find_initial_leaf(root, co);
    const Node *current_node = &initial_leaf;
    bool just_went_down = true;

    Stack<const InnerNode *> finished_inner_nodes;

    while (current_node != nullptr) {
      if (current_node->type == NodeType::Leaf) {
        const LeafNode &leaf_node = *static_cast<const LeafNode *>(current_node);
        for (const Point &point : leaf_node.points) {
          const float distance_sq = this->calc_distance_sq(co, point);
          if (distance_sq <= *r_max_distance_sq) {
            float new_max_distance_sq = *r_max_distance_sq;
            func(point, distance_sq, &new_max_distance_sq);
            BLI_assert(new_max_distance_sq <= *r_max_distance_sq);
            *r_max_distance_sq = new_max_distance_sq;
          }
        }
        current_node = current_node->parent;
        just_went_down = false;
      }
      else {
        const InnerNode &inner_node = *static_cast<const InnerNode *>(current_node);
        const float co_in_dim = co[inner_node.dim];
        const float signed_split_distance = co_in_dim - inner_node.value;
        const int initial_child = signed_split_distance > 0.0f;
        if (just_went_down) {
          this->prefetch_child_data(inner_node, initial_child);
          current_node = inner_node.children[initial_child];
        }
        else {
          if (!finished_inner_nodes.is_empty() && finished_inner_nodes.peek() == &inner_node) {
            finished_inner_nodes.pop();
            current_node = inner_node.parent;
          }
          else {
            const float split_distance_sq = signed_split_distance * signed_split_distance;
            if (split_distance_sq <= *r_max_distance_sq) {
              const int other_child = 1 - initial_child;
              this->prefetch_child_data(inner_node, other_child);
              current_node = inner_node.children[other_child];
              just_went_down = true;
              finished_inner_nodes.push(&inner_node);
            }
            else {
              current_node = inner_node.parent;
            }
          }
        }
      }
    }
  }

  const LeafNode &find_initial_leaf(const Node &root, const float *co) const
  {
    const Node *current = &root;
    while (current->type == NodeType::Inner) {
      const InnerNode &inner_node = *static_cast<const InnerNode *>(current);
      const int child_index = co[inner_node.dim] > inner_node.value;
      this->prefetch_child_data(inner_node, child_index);
      current = inner_node.children[child_index];
    }
    return *static_cast<const LeafNode *>(current);
  }

  void prefetch_child_data(const InnerNode &node, const int child_index) const
  {
    _mm_prefetch(node.prefetch_pointers[child_index][0], _MM_HINT_T0);
    _mm_prefetch(node.prefetch_pointers[child_index][1], _MM_HINT_T0);
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
      ss << ")\n";
    }
    dot::Node &dot_node = digraph.new_node(ss.str());
    dot_node.set_shape(dot::Attr_shape::Rectangle);
    return dot_node;
  }
};

}  // namespace blender::kdtree
