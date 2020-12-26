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

#include <algorithm>

#include "BLI_dot_export.hh"
#include "BLI_float3.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_span.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender::octree {

template<typename Point> struct DefaultPointAdapter {
};

struct Float3PointAdapter {
  float get(const float3 &value, const int dim) const
  {
    return value[dim];
  }

  float3 get(const float3 &value) const
  {
    return value;
  }
};

template<> struct DefaultPointAdapter<float3> {
  using type = Float3PointAdapter;
};

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

struct InnerNode : public Node {
  float3 center;
  std::array<Node *, 8> children;

  InnerNode() : Node(NodeType::Inner)
  {
  }
};

template<typename Point> struct LeafNode_ : public Node {
  MutableSpan<Point> points;

  LeafNode_() : Node(NodeType::Leaf)
  {
  }
};

struct BoundingBox {
  float3 min, max;

  float3 center() const
  {
    return (max + min) * 0.5f;
  }
};

template<typename Point, typename PointAdapter = typename DefaultPointAdapter<Point>::type>
class Octree : NonCopyable, NonMovable {
 private:
  using LeafNode = LeafNode_<Point>;

  LinearAllocator<> allocator_;
  PointAdapter adapter_;
  InnerNode *root_ = nullptr;

 public:
  Octree(Span<Point> points, PointAdapter adapter = {}) : adapter_(adapter)
  {
    root_ = &this->build_tree_from_root(points);
  }

  std::string to_dot() const
  {
    dot::DirectedGraph digraph;
    digraph.set_rankdir(dot::Attr_rankdir::TopToBottom);
    this->make_dot_nodes(digraph, *root_);
    return digraph.to_dot_string();
  }

 private:
  InnerNode &build_tree_from_root(const Span<Point> points)
  {
    const BoundingBox bbox = this->compute_bounding_box(points);
    return this->build_inner_node(points, bbox);
  }

  Node &build_tree(const Span<Point> points, const BoundingBox &bbox)
  {
    if (points.size() < 30) {
      return this->build_leaf_node(points);
    }
    return this->build_inner_node(points, bbox);
  }

  LeafNode &build_leaf_node(const Span<Point> points)
  {
    LeafNode &node = *allocator_.construct<LeafNode>();
    node.points = allocator_.construct_array_copy(points);
    return node;
  }

  InnerNode &build_inner_node(const Span<Point> points, const BoundingBox &bbox)
  {
    const float3 center = bbox.center();
    std::array<Vector<Point>, 8> sub_points = this->split_points(points, center);
    InnerNode &node = *allocator_.construct<InnerNode>();
    node.center = center;
    for (const int i : IndexRange(8)) {
      BoundingBox sub_bbox;
      for (const int dim : IndexRange(3)) {
        if ((i & (1 << dim)) == 0) {
          sub_bbox.min[dim] = bbox.min[dim];
          sub_bbox.max[dim] = center[dim];
        }
        else {
          sub_bbox.min[dim] = center[dim];
          sub_bbox.max[dim] = bbox.max[dim];
        }
      }
      node.children[i] = &this->build_tree(sub_points[i], sub_bbox);
    }
    return node;
  }

  std::array<Vector<Point>, 8> split_points(const Span<Point> points, const float3 center) const
  {
    std::array<Vector<Point>, 8> sub_points;
    for (const Point &point : points) {
      const int x_is_larger = adapter_.get(point, 0) > center.x;
      const int y_is_larger = adapter_.get(point, 1) > center.y;
      const int z_is_larger = adapter_.get(point, 2) > center.z;
      const int sub_index = x_is_larger | (y_is_larger << 1) | (z_is_larger << 2);
      sub_points[sub_index].append(point);
    }
    return sub_points;
  }

  BoundingBox compute_bounding_box(const Span<Point> points) const
  {
    BoundingBox bbox;
    bbox.min = float3(FLT_MAX);
    bbox.max = float3(-FLT_MAX);

    for (const Point &point : points) {
      const float3 co = adapter_.get(point);
      minmax_v3v3_v3(bbox.min, bbox.max, co);
    }

    return bbox;
  }

  dot::Node &make_dot_nodes(dot::DirectedGraph &digraph, const Node &node) const
  {
    if (node.type == NodeType::Inner) {
      const InnerNode &inner_node = static_cast<const InnerNode &>(node);
      std::stringstream ss;
      ss << inner_node.center;
      dot::Node &dot_node = digraph.new_node(ss.str());
      dot_node.set_shape(dot::Attr_shape::Rectangle);
      dot_node.attributes.set("ordering", "out");
      for (const int i : IndexRange(8)) {
        dot::Node &child = this->make_dot_nodes(digraph, *inner_node.children[i]);
        digraph.new_edge(dot_node, child);
      }
      return dot_node;
    }
    else {
      const LeafNode &leaf_node = static_cast<const LeafNode &>(node);
      std::stringstream ss;
      for (const Point &point : leaf_node.points) {
        ss << point << "\n";
      }
      dot::Node &dot_node = digraph.new_node(ss.str());
      dot_node.set_shape(dot::Attr_shape::Rectangle);
      return dot_node;
    }
  }
};

}  // namespace blender::octree
