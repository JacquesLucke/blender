/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_spline.hh"

#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_deform_curves_with_surface_cc {

using blender::attribute_math::mix2;

NODE_STORAGE_FUNCS(NodeGeometryCurveTrim)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curves")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Vector>(N_("Rest Position")).hide_value().supports_field();
  b.add_output<decl::Geometry>(N_("Curves"));
}

static void node_layout(uiLayout *UNUSED(layout), bContext *UNUSED(C), PointerRNA *UNUSED(ptr))
{
}

static void node_init(bNodeTree *UNUSED(tree), bNode *UNUSED(node))
{
}

static void node_update(bNodeTree *UNUSED(ntree), bNode *UNUSED(node))
{
}

static void deform_curves(const MeshComponent &mesh_component,
                          const Span<float3> rest_positions,
                          CurveComponent &curves_component)
{
  bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(
      curves_component.get_for_write()->geometry);

  const MutableSpan<float3> positions = curves.positions_for_write();
  const VArray<int> surface_triangle_indices_varray = curves.surface_triangle_indices();
  if (surface_triangle_indices_varray.is_single() &&
      surface_triangle_indices_varray.get_internal_single() < 0) {
    return;
  }
  const VArray_Span<int> surface_triangle_indices{surface_triangle_indices_varray};
  const Span<float2> surface_triangle_coordinates = curves.surface_triangle_coords();
  if (surface_triangle_coordinates.is_empty()) {
    return;
  }

  const Mesh &mesh = *mesh_component.get_for_read();
  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&mesh),
                                BKE_mesh_runtime_looptri_len(&mesh)};

  threading::parallel_for(curves.curves_range(), 512, [&](const IndexRange curves_range) {
    for (const int curve_i : curves_range) {
      const int looptri_i = surface_triangle_indices[curve_i];
      if (looptri_i < 0 || looptri_i >= looptris.size()) {
        continue;
      }
      const IndexRange points = curves.points_for_curve(curve_i);
      const int root_point_i = points[0];

      const float3 bary_coord = bke::curves::decode_surface_bary_coord(
          surface_triangle_coordinates[curve_i]);
      const MLoopTri &looptri = looptris[looptri_i];
      const int v0 = mesh.mloop[looptri.tri[0]].v;
      const int v1 = mesh.mloop[looptri.tri[1]].v;
      const int v2 = mesh.mloop[looptri.tri[2]].v;

      const float3 &rest_v0 = rest_positions[v0];
      const float3 &rest_v1 = rest_positions[v1];
      const float3 &rest_v2 = rest_positions[v2];

      const float3 &deformed_v0 = mesh.mvert[v0].co;
      const float3 &deformed_v1 = mesh.mvert[v1].co;
      const float3 &deformed_v2 = mesh.mvert[v2].co;

      float3 old_normal;
      normal_tri_v3(old_normal, rest_v0, rest_v1, rest_v2);
      float3 new_normal;
      normal_tri_v3(new_normal, deformed_v0, deformed_v1, deformed_v2);

      float rotation_mat[3][3];
      rotation_between_vecs_to_mat3(rotation_mat, old_normal, new_normal);

      const float3 old_curve_root = positions[root_point_i];
      const float3 old_surface_pos = attribute_math::mix3(bary_coord, rest_v0, rest_v1, rest_v2);
      const float3 new_surface_pos = attribute_math::mix3(
          bary_coord, deformed_v0, deformed_v1, deformed_v2);
      const float3 root_pos_diff = new_surface_pos - old_surface_pos;

      for (const int point_i : points) {
        const float3 old_pos = positions[point_i];
        const float3 old_relative_pos = old_pos - old_surface_pos;
        float3 new_relative_pos = old_relative_pos;
        mul_m3_v3(rotation_mat, new_relative_pos);
        const float3 new_pos = new_surface_pos + new_relative_pos;
        positions[point_i] = new_pos;
      }
    }
  });

  curves.tag_positions_changed();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet curves_geometry = params.extract_input<GeometrySet>("Curves");
  GeometrySet mesh_geometry = params.extract_input<GeometrySet>("Mesh");
  Field<float3> rest_positions_field = params.extract_input<Field<float3>>("Rest Position");

  if (!mesh_geometry.has_mesh()) {
    params.set_output("Curves", std::move(curves_geometry));
    return;
  }

  const MeshComponent &mesh_component = *mesh_geometry.get_component_for_read<MeshComponent>();
  GeometryComponentFieldContext field_context{mesh_component, ATTR_DOMAIN_POINT};
  FieldEvaluator field_evaluator{field_context,
                                 mesh_component.attribute_domain_size(ATTR_DOMAIN_POINT)};
  field_evaluator.add(rest_positions_field);
  field_evaluator.evaluate();
  const VArray_Span<float3> rest_positions = field_evaluator.get_evaluated<float3>(0);

  curves_geometry.modify_geometry_sets([&](GeometrySet &curve_geometry) {
    if (!curve_geometry.has_curves()) {
      return;
    }
    CurveComponent &curves_component = curves_geometry.get_component_for_write<CurveComponent>();
    deform_curves(mesh_component, rest_positions, curves_component);
  });

  params.set_output("Curves", std::move(curves_geometry));
}

}  // namespace blender::nodes::node_geo_deform_curves_with_surface_cc

void register_node_type_geo_deform_curves_with_surface()
{
  namespace file_ns = blender::nodes::node_geo_deform_curves_with_surface_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype,
                     GEO_NODE_DEFORM_CURVES_WITH_SURFACE,
                     "Deform Curves with Surface",
                     NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.declare = file_ns::node_declare;
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  nodeRegisterType(&ntype);
}
