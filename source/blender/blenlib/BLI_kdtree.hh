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
#include "BLI_linear_allocator.hh"
#include "BLI_rand.hh"
#include "BLI_stack.hh"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_timeit.hh"
#include "BLI_utildefines.h"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"
#include "BLI_vector_adaptor.hh"

namespace blender::kdtree {

enum class NodeType {
  Inner,
  Leaf,
};

struct SplitInfo {
  int dim;
  float value;
};

struct ThreeSplitsInfo {
  SplitInfo a;
  std::array<SplitInfo, 2> b;
  std::array<SplitInfo, 4> c;
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
  SplitInfo split;
  std::array<Node *, 2> children;
  std::array<std::array<const void *, 2>, 2> prefetch_pointers = {0};

  InnerNode() : Node(NodeType::Inner)
  {
  }
};

template<typename Point> class TemporaryPointBuffersCache : NonCopyable, NonMovable {
 public:
  TemporaryPointBuffersCache() = default;
  ~TemporaryPointBuffersCache() = default;

  VectorAdaptor<Point> allocate(const int size)
  {
    void *buffer = MEM_malloc_arrayN(size, sizeof(Point), "kdtree temporary buffer");
    return VectorAdaptor{static_cast<Point *>(buffer), size};
  }

  void deallocate(Point *buffer)
  {
    MEM_freeN(buffer);
  }

  void destruct_and_deallocate(Span<MutableSpan<Point>> buffers)
  {
    for (MutableSpan<Point> buffer : buffers) {
      destruct_n(buffer.data(), buffer.size());
      this->deallocate(buffer.data());
    }
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
  using BufferCache = TemporaryPointBuffersCache<Point>;
  static inline constexpr int DIM = PointAdapater::DIM;
  static inline constexpr int MAX_LEAF_SIZE = MaxLeafSize;

  EnumerableThreadSpecific<LinearAllocator<>> allocators_;
  PointAdapater adapter_;
  Node *root_ = nullptr;

  SpinLock leaf_point_buffers_lock_;
  Vector<MutableSpan<Point>> leaf_point_buffers_;

 public:
  KDTree(Span<Point> points, PointAdapater adapter = {}) : adapter_(adapter)
  {
    BLI_spin_init(&leaf_point_buffers_lock_);

    BufferCache buffer_cache;
    root_ = this->build_tree(buffer_cache, {points}, {});
    this->set_parent_and_prefetch_pointers(*root_);
  }

  ~KDTree()
  {
    if (root_ != nullptr) {
      this->free_leaf_point_buffers();
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
    const LeafNode &initial_leaf = this->find_initial_leaf(*root_, co);
    this->foreach_in_shrinking_radius_internal(initial_leaf, co, func, &max_distance_sq);
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
        if (adapter_.get(point, inner_node.split.dim) > inner_node.split.value) {
          std::cout << "error: " << point << "\n";
        }
      });
      this->foreach_point(*inner_node.children[1], [&](const Point &point) {
        if (adapter_.get(point, inner_node.split.dim) < inner_node.split.value) {
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
  }

 private:
  BLI_NOINLINE Node *build_tree(BufferCache &buffer_cache,
                                Span<Span<Point>> point_spans,
                                Span<MutableSpan<Point>> owning_spans)
  {
    int tot_points = 0;
    for (Span<Point> points : point_spans) {
      tot_points += points.size();
    }

    if (tot_points <= MaxLeafSize * 128) {
      MutableSpan<Point> stored_points = this->create_leaf_points_buffer(tot_points, point_spans);
      buffer_cache.destruct_and_deallocate(owning_spans);
      LinearAllocator<> &allocator = allocators_.local();
      return this->build_tree__last_levels(stored_points, allocator);
    }
    return this->build_tree__three_levels(buffer_cache, tot_points, point_spans, owning_spans);
  }

  BLI_NOINLINE Node *build_tree__three_levels(BufferCache &buffer_cache,
                                              const int tot_points,
                                              Span<Span<Point>> point_spans,
                                              Span<MutableSpan<Point>> owning_spans)
  {
    LinearAllocator<> &allocator = allocators_.local();
    InnerNode *inner1 = allocator.construct<InnerNode>();
    std::array<InnerNode *, 2> inner2 = {allocator.construct<InnerNode>(),
                                         allocator.construct<InnerNode>()};
    std::array<InnerNode *, 4> inner3{allocator.construct<InnerNode>(),
                                      allocator.construct<InnerNode>(),
                                      allocator.construct<InnerNode>(),
                                      allocator.construct<InnerNode>()};

    inner1->children = {inner2[0], inner2[1]};
    inner2[0]->children = {inner3[0], inner3[1]};
    inner2[1]->children = {inner3[2], inner3[3]};

    const int sample_size = std::min<int>(2000, std::max(100, tot_points / 100));
    Array<Point> point_samples = this->get_random_samples(point_spans, sample_size);
    inner1->split = this->find_splitter_exact(point_samples);

    std::array<MutableSpan<Point>, 2> split_points1;
    this->split_points(point_samples, inner1->split, &split_points1[0], &split_points1[1]);

    inner2[0]->split = this->find_splitter_exact(split_points1[0]);
    inner2[1]->split = this->find_splitter_exact(split_points1[1]);

    std::array<MutableSpan<Point>, 4> split_points2;
    this->split_points(split_points1[0], inner2[0]->split, &split_points2[0], &split_points2[1]);
    this->split_points(split_points1[1], inner2[1]->split, &split_points2[2], &split_points2[3]);

    inner3[0]->split = this->find_splitter_exact(split_points2[0]);
    inner3[1]->split = this->find_splitter_exact(split_points2[1]);
    inner3[2]->split = this->find_splitter_exact(split_points2[2]);
    inner3[3]->split = this->find_splitter_exact(split_points2[3]);

    ThreeSplitsInfo three_splits = {
        inner1->split,
        {inner2[0]->split, inner2[1]->split},
        {inner3[0]->split, inner3[1]->split, inner3[2]->split, inner3[3]->split}};

    std::array<Vector<MutableSpan<Point>>, 8> point_buckets = this->split_points_three_times(
        tot_points, buffer_cache, point_spans, three_splits);
    buffer_cache.destruct_and_deallocate(owning_spans);

    tbb::task_group tasks;

    for (const int i : IndexRange(8)) {
      tasks.run([&, i]() {
        Vector<MutableSpan<Point>> &owning_spans_for_child = point_buckets[i];
        Vector<Span<Point>> spans_for_child = owning_spans_for_child.as_span();
        inner3[i / 2]->children[i % 2] = this->build_tree(
            buffer_cache, spans_for_child, owning_spans_for_child);
      });
    }

    tasks.wait();

    return inner1;
  }

  BLI_NOINLINE Node *build_tree__last_levels(MutableSpan<Point> points,
                                             LinearAllocator<> &allocator)
  {
    if (points.size() <= MAX_LEAF_SIZE) {
      return this->build_tree__leaf(points, allocator);
    }
    if (points.size() > 100) {
      return this->build_tree__last_levels_approximate(points, allocator);
    }

    InnerNode *node = allocator.construct<InnerNode>();
    node->split.dim = this->find_best_split_dim(points);
    const int median_pos = points.size() / 2;
    this->sort_around_nth_element(points, median_pos, node->split.dim);

    node->split.value = adapter_.get(points[median_pos], node->split.dim);
    node->children[0] = this->build_tree__last_levels(points.take_front(median_pos), allocator);
    node->children[1] = this->build_tree__last_levels(points.drop_front(median_pos), allocator);
    return node;
  }

  BLI_NOINLINE Node *build_tree__leaf(MutableSpan<Point> points, LinearAllocator<> &allocator)
  {
    LeafNode *node = allocator.construct<LeafNode>();
    node->points = points;
    return node;
  }

  BLI_NOINLINE Node *build_tree__last_levels_approximate(MutableSpan<Point> points,
                                                         LinearAllocator<> &allocator)
  {
    InnerNode *node = allocator.construct<InnerNode>();
    node->split = this->find_splitter_approximate(points);

    MutableSpan<Point> left_points, right_points;
    this->split_points(points, node->split, &left_points, &right_points);

    node->children[0] = this->build_tree__last_levels(left_points, allocator);
    node->children[1] = this->build_tree__last_levels(right_points, allocator);

    return node;
  }

  BLI_NOINLINE SplitInfo find_splitter_approximate(MutableSpan<Point> points) const
  {
    if (points.size() < 50) {
      return this->find_splitter_exact(points);
    }

    const int sample_size = std::max<int>(20, points.size() / 100);
    Array<Point> point_samples = this->get_random_samples(points, sample_size);
    return this->find_splitter_exact(point_samples);
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

  BLI_NOINLINE Array<Point> get_random_samples(Span<Span<Point>> points, const int amount) const
  {
    /* TODO: Take samples from all spans. */
    return this->get_random_samples(points[0], amount);
  }

  BLI_NOINLINE SplitInfo find_splitter_exact(MutableSpan<Point> points) const
  {
    SplitInfo split;
    split.dim = this->find_best_split_dim(points);
    split.value = this->find_best_split_value(points, split.dim);
    return split;
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

  BLI_NOINLINE float find_best_split_value(MutableSpan<Point> points, const int dim) const
  {
    const int median_position = points.size() / 2;
    this->sort_around_nth_element(points, median_position, dim);
    return adapter_.get(points[median_position], dim);
  }

  BLI_NOINLINE void sort_around_nth_element(MutableSpan<Point> points,
                                            const int index,
                                            const int dim) const
  {
    BLI_assert(index < points.size());
    std::nth_element(
        points.begin(), points.begin() + index, points.end(), [&](const Point &a, const Point &b) {
          return adapter_.get(a, dim) < adapter_.get(b, dim);
        });
  }

  BLI_NOINLINE std::array<Vector<MutableSpan<Point>>, 8> split_points_three_times(
      const int tot_points,
      BufferCache &buffer_cache,
      Span<Span<Point>> point_spans,
      const ThreeSplitsInfo &splits) const
  {
    std::array<Vector<MutableSpan<Point>>, 8> output_buckets;
    this->split_points_in_temporary_buffers(
        tot_points, buffer_cache, point_spans, splits, output_buckets);
    return output_buckets;
  }

  BLI_NOINLINE void split_points_in_temporary_buffers(
      const int tot_points,
      BufferCache &buffer_cache,
      Span<Span<Point>> point_spans,
      const ThreeSplitsInfo &splits,
      std::array<Vector<MutableSpan<Point>>, 8> &r_buckets) const
  {
    std::array<VectorAdaptor<Point>, 8> current_buckets;
    const int buffer_size = tot_points / 6;
    const int almost_full_size = buffer_size * 0.8;

    auto smallest_remaining_capacity = [&]() {
      int min_capacity = INT32_MAX;
      for (const VectorAdaptor<Point> &bucket : current_buckets) {
        if (bucket.remaining_capacity() < min_capacity) {
          min_capacity = bucket.remaining_capacity();
        }
      }
      return min_capacity;
    };

    for (VectorAdaptor<Point> &bucket : current_buckets) {
      bucket = buffer_cache.allocate(buffer_size);
    }

    for (Span<Point> points : point_spans) {
      while (!points.is_empty()) {
        const int min_capacity = smallest_remaining_capacity();
        const int chunk_size = std::min<int>(min_capacity, points.size());
        Span<Point> sub_points = points.take_front(chunk_size);
        points = points.drop_front(chunk_size);

        std::array<Point *, 8> end_pointers;
        for (const int i : IndexRange(8)) {
          end_pointers[i] = current_buckets[i].end();
        }
        this->move_points_to_buckets(sub_points, end_pointers, splits);
        for (const int i : IndexRange(8)) {
          current_buckets[i].set_end(end_pointers[i]);
        }

        if (points.size() > 0) {
          for (const int i : IndexRange(8)) {
            VectorAdaptor<Point> &bucket = current_buckets[i];
            if (bucket.size() >= almost_full_size) {
              r_buckets[i].append(bucket);
              bucket = buffer_cache.allocate(buffer_size);
            }
          }
        }
      }
    }

    for (const int bucket_index : IndexRange(8)) {
      MutableSpan<Point> current_bucket = current_buckets[bucket_index];
      if (current_bucket.is_empty()) {
        buffer_cache.deallocate(current_bucket.data());
      }
      else {
        r_buckets[bucket_index].append(current_bucket);
      }
    }
  }

  BLI_NOINLINE void move_points_to_buckets(Span<Point> points,
                                           std::array<Point *, 8> &buckets,
                                           const ThreeSplitsInfo &splits) const
  {
    // std::cout << points.size() << "\n";
    // SCOPED_TIMER(__func__);
    for (const Point &point : points) {
      const int bucket_index = this->compute_bucket_index(point, splits);
      new (buckets[bucket_index]++) Point(point);
    }
  }

  int compute_bucket_index(const Point &point, const ThreeSplitsInfo &splits) const
  {
    const int i1 = adapter_.get(point, splits.a.dim) > splits.a.value;
    const int i2 = adapter_.get(point, splits.b[i1].dim) > splits.b[i1].value;
    const int i3 = adapter_.get(point, splits.c[i1 * 2 + i2].dim) > splits.c[i1 * 2 + i2].value;
    const int bucket_index = i1 * 4 + i2 * 2 + i3;
    return bucket_index;
  }

  BLI_NOINLINE void split_points(MutableSpan<Point> points,
                                 const SplitInfo split,
                                 MutableSpan<Point> *r_left_points,
                                 MutableSpan<Point> *r_right_points)
  {
    auto is_larger_than_split_value = [&](const Point &p) {
      return adapter_.get(p, split.dim) > split.value;
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
      if (adapter_.get(points[i], split.dim) <= split.value) {
        i++;
      }
      else {
        break;
      }
    }
    while (i > best_i && i > 0) {
      if (adapter_.get(points[i - 1], split.dim) >= split.value) {
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
      if (adapter_.get(point, split.dim) > split.value) {
        BLI_assert(false);
      }
    }
    for (const Point &point : *r_right_points) {
      if (adapter_.get(point, split.dim) < split.value) {
        BLI_assert(false);
      }
    }
#endif
  }

  BLI_NOINLINE MutableSpan<Point> create_leaf_points_buffer(const int tot_points,
                                                            Span<Span<Point>> point_spans)
  {
    void *buffer = MEM_malloc_arrayN(tot_points, sizeof(Point), __func__);
    MutableSpan<Point> leaf_points{static_cast<Point *>(buffer), tot_points};
    int offset = 0;
    for (Span<Point> points : point_spans) {
      uninitialized_copy_n(points.data(), points.size(), leaf_points.data() + offset);
      offset += points.size();
    }

    BLI_spin_lock(&leaf_point_buffers_lock_);
    leaf_point_buffers_.append(leaf_points);
    BLI_spin_unlock(&leaf_point_buffers_lock_);

    return leaf_points;
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

  BLI_NOINLINE void free_leaf_point_buffers()
  {
    for (MutableSpan<Point> points : leaf_point_buffers_) {
      destruct_n(points.data(), points.size());
      MEM_freeN(points.data());
    }
  }

  BLI_NOINLINE void free_tree(Node *UNUSED(node))
  {
    /* Nodes are all freed together by linear allocators. */
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
  BLI_NOINLINE void foreach_in_shrinking_radius_internal(const LeafNode &initial_leaf,
                                                         const float *co,
                                                         const Func &func,
                                                         float *r_max_distance_sq) const
  {
    this->foreach_in_shrinking_radius_handle_leaf(initial_leaf, co, func, r_max_distance_sq);

    const Node *current_node = initial_leaf.parent;
    bool just_went_down = false;

    Stack<const InnerNode *> finished_inner_nodes;

    while (current_node != nullptr) {
      if (current_node->type == NodeType::Leaf) {
        const LeafNode &leaf_node = *static_cast<const LeafNode *>(current_node);
        this->foreach_in_shrinking_radius_handle_leaf(leaf_node, co, func, r_max_distance_sq);
        current_node = current_node->parent;
        just_went_down = false;
      }
      else {
        const InnerNode &inner_node = *static_cast<const InnerNode *>(current_node);
        const float co_in_dim = co[inner_node.split.dim];
        const float signed_split_distance = co_in_dim - inner_node.split.value;
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

  template<typename Func>
  BLI_NOINLINE void foreach_in_shrinking_radius_handle_leaf(const LeafNode &node,
                                                            const float *co,
                                                            const Func &func,
                                                            float *r_max_distance_sq) const
  {
    for (const Point &point : node.points) {
      const float distance_sq = this->calc_distance_sq(co, point);
      if (distance_sq <= *r_max_distance_sq) {
        float new_max_distance_sq = *r_max_distance_sq;
        func(point, distance_sq, &new_max_distance_sq);
        BLI_assert(new_max_distance_sq <= *r_max_distance_sq);
        *r_max_distance_sq = new_max_distance_sq;
      }
    }
  }

  const LeafNode &find_initial_leaf(const Node &root, const float *co) const
  {
    const Node *current = &root;
    while (current->type == NodeType::Inner) {
      const InnerNode &inner_node = *static_cast<const InnerNode *>(current);
      const int child_index = co[inner_node.split.dim] > inner_node.split.value;
      this->prefetch_child_data(inner_node, child_index);
      current = inner_node.children[child_index];
    }
    return *static_cast<const LeafNode *>(current);
  }

  void prefetch_child_data(const InnerNode &node, const int child_index) const
  {
    _mm_prefetch(static_cast<const char *>(node.prefetch_pointers[child_index][0]), _MM_HINT_T0);
    _mm_prefetch(static_cast<const char *>(node.prefetch_pointers[child_index][1]), _MM_HINT_T0);
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
    const std::string name = "[" + std::to_string(node.split.dim) +
                             "] = " + std::to_string(node.split.value);
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
