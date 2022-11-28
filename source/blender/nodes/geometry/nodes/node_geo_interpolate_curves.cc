/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_kdtree.h"
#include "BLI_length_parameterize.hh"
#include "BLI_task.hh"

#include "BKE_curves.hh"

#include "DNA_pointcloud_types.h"

namespace blender::nodes::node_geo_interpolate_curves_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Points"));
  b.add_input<decl::Geometry>(N_("Guide Curves"));
  b.add_output<decl::Geometry>(N_("Curves"));
}

struct NeighborCurve {
  int index;
  float weight;
};
using NeighborCurves = Vector<NeighborCurve, 5>;

static Array<NeighborCurves> find_neighbor_guides(const Span<float3> positions,
                                                  const KDTree_3d &guide_roots_kdtree,
                                                  const int max_neighbor_count,
                                                  const float max_neighbor_distance)
{
  Array<NeighborCurves> neighbors_per_point(positions.size());
  threading::parallel_for(positions.index_range(), 128, [&](const IndexRange range) {
    for (const int i : range) {
      const float3 &position = positions[i];

      NeighborCurves &neighbors = neighbors_per_point[i];

      Vector<KDTreeNearest_3d, 16> nearest_n(max_neighbor_count);
      const int num_neighbors = BLI_kdtree_3d_find_nearest_n(
          &guide_roots_kdtree, position, nearest_n.data(), max_neighbor_count);
      if (num_neighbors == 0) {
        continue;
      }
      const float max_distance = std::max_element(
                                     nearest_n.begin(),
                                     nearest_n.begin() + num_neighbors,
                                     [](const KDTreeNearest_3d &a, const KDTreeNearest_3d &b) {
                                       return a.dist < b.dist;
                                     })
                                     ->dist;
      if (max_distance == 0.0f) {
        neighbors.append({nearest_n[0].index, 1.0f});
        continue;
      }
      float tot_weight = 0.0f;
      for (const int neighbor_i : IndexRange(num_neighbors)) {
        const KDTreeNearest_3d &nearest = nearest_n[neighbor_i];
        /* Goal for this weight calculation:
         * - As distance gets closer to zero, it should become very large.
         * - At `max_distance` the weight should be zero.
         */
        const float weight = (max_distance - nearest.dist) / std::max(nearest.dist, 0.000001f);
        if (weight > 0.0f) {
          tot_weight += weight;
          neighbors.append({nearest.index, weight});
        }
      }
      if (tot_weight > 0.0f) {
        const float weight_factor = 1.0f / tot_weight;
        for (NeighborCurve &neighbor : neighbors) {
          neighbor.weight *= weight_factor;
        }
      }
    }
  });
  return neighbors_per_point;
}

static GeometrySet generate_interpolated_curves(const Curves &guide_curves_id,
                                                const PointCloud &points)
{
  const bke::CurvesGeometry &guide_curves = bke::CurvesGeometry::wrap(guide_curves_id.geometry);
  const Span<float3> guide_positions = guide_curves.positions();
  const Span<int> guide_offsets = guide_curves.offsets();

  KDTree_3d *guide_roots_kdtree = BLI_kdtree_3d_new(guide_curves.curves_num());
  BLI_SCOPED_DEFER([&]() { BLI_kdtree_3d_free(guide_roots_kdtree); });

  for (const int curve_i : guide_curves.curves_range()) {
    const IndexRange points = guide_curves.points_for_curve(curve_i);
    const int first_point_i = points[0];
    const float3 &root_pos = guide_positions[first_point_i];
    BLI_kdtree_3d_insert(guide_roots_kdtree, curve_i, root_pos);
  }

  BLI_kdtree_3d_balance(guide_roots_kdtree);

  const AttributeAccessor guide_curve_attributes = guide_curves.attributes();
  const AttributeAccessor point_attributes = points.attributes();

  const VArraySpan point_positions = point_attributes.lookup<float3>("position");

  const Array<NeighborCurves> neighbors_per_point = find_neighbor_guides(
      point_positions, *guide_roots_kdtree, 5, 100000.0f);

  const int points_per_child = 8;
  const int num_child_curves = points.totpoint;
  const int num_child_points = num_child_curves * points_per_child;

  Curves *child_curves_id = bke::curves_new_nomain(num_child_points, num_child_curves);
  bke::CurvesGeometry &child_curves = bke::CurvesGeometry::wrap(child_curves_id->geometry);
  MutableSpan<int> children_curves_offsets = child_curves.offsets_for_write();
  MutableSpan<float3> children_curves_positions = child_curves.positions_for_write();

  threading::parallel_for(IndexRange(num_child_curves), 128, [&](const IndexRange range) {
    for (const int child_curve_i : range) {
      children_curves_offsets[child_curve_i] = child_curve_i * points_per_child;
      const IndexRange points(child_curve_i * points_per_child, points_per_child);
      const NeighborCurves &neighbors = neighbors_per_point[child_curve_i];

      const float3 &child_root_position = point_positions[child_curve_i];

      MutableSpan<float3> child_positions = children_curves_positions.slice(points);

      child_positions.fill(child_root_position);
      if (neighbors.is_empty()) {
        continue;
      }

      for (const NeighborCurve &neighbor : neighbors) {
        const IndexRange guide_points = guide_curves.points_for_curve(neighbor.index);
        const Span<float3> neighbor_positions = guide_positions.slice(guide_points);
        const float3 &neighbor_root = neighbor_positions[0];

        Array<float, 32> lengths(length_parameterize::segments_num(guide_points.size(), false));
        length_parameterize::accumulate_lengths<float3>(neighbor_positions, false, lengths);
        const float neighbor_length = lengths.last();

        length_parameterize::SampleSegmentHint sample_hint;
        for (const int i : IndexRange(points.size())) {
          const float sample_length = i * neighbor_length * (1.0f / float(points.size() - 1));
          int segment_index;
          float factor;
          length_parameterize::sample_at_length(
              lengths, sample_length, segment_index, factor, &sample_hint);

          const float3 sample_pos = math::interpolate(
              neighbor_positions[segment_index], neighbor_positions[segment_index + 1], factor);

          child_positions[i] += neighbor.weight * (sample_pos - neighbor_root);
        }
      }
    }
  });

  children_curves_offsets.last() = num_child_points;

  child_curves.fill_curve_types(CURVE_TYPE_CATMULL_ROM);

  return GeometrySet::create_with_curves(child_curves_id);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const GeometrySet guide_curves_geometry = params.extract_input<GeometrySet>("Guide Curves");
  const GeometrySet points_geometry = params.extract_input<GeometrySet>("Points");

  if (!guide_curves_geometry.has_curves()) {
    params.set_default_remaining_outputs();
    return;
  }
  if (!points_geometry.has_pointcloud()) {
    params.set_default_remaining_outputs();
    return;
  }

  const Curves &guide_curves_id = *guide_curves_geometry.get_curves_for_read();
  const PointCloud &points = *points_geometry.get_pointcloud_for_read();

  GeometrySet new_curves = generate_interpolated_curves(guide_curves_id, points);
  params.set_output("Curves", std::move(new_curves));
}

}  // namespace blender::nodes::node_geo_interpolate_curves_cc

void register_node_type_geo_interpolate_curves()
{
  namespace file_ns = blender::nodes::node_geo_interpolate_curves_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INTERPOLATE_CURVES, "Interpolate Curves", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
