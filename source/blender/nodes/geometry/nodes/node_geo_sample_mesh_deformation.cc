/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_type_conversions.hh"

#include "BLI_float3x3.hh"
#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "NOD_socket_search_link.hh"

#include "GEO_reverse_uv_sampler.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_sample_mesh_deformation_cc {

using attribute_math::mix3;
using geometry::ReverseUVSampler;

NODE_STORAGE_FUNCS(NodeGeometryCurveTrim)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Vector>(N_("Rest Position")).hide_value().supports_field();
  b.add_input<decl::Vector>(N_("UV Map")).hide_value().supports_field();
  b.add_input<decl::Vector>(N_("Sample UV")).supports_field();
  b.add_output<decl::Vector>(N_("Translation")).dependent_field({3});
  b.add_output<decl::Vector>(N_("Rotation")).dependent_field({3});
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

class SampleMeshDeformationFunction : public fn::MultiFunction {
 private:
  GeometrySet geometry_;
  const Mesh *mesh_;
  Span<MVert> verts_;
  Array<float3> rest_positions_;
  Array<float2> uv_map_;
  std::optional<ReverseUVSampler> reverse_uv_sampler;

 public:
  SampleMeshDeformationFunction(GeometrySet geometry,
                                VArray<float3> rest_positions,
                                VArray<float2> uv_map)
      : geometry_(std::move(geometry)),
        rest_positions_(VArray_Span(rest_positions)),
        uv_map_(VArray_Span(uv_map))
  {
    mesh_ = geometry_.get_mesh_for_read();
    verts_ = Span<MVert>(mesh_->mvert, mesh_->totvert);
    const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(mesh_),
                                  BKE_mesh_runtime_looptri_len(mesh_)};
    reverse_uv_sampler.emplace(uv_map_, looptris);

    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Sample Mesh Deformation"};
    signature.single_input<float2>("Sample UV");
    signature.single_output<float3>("Translation");
    signature.single_output<float3>("Rotation");
    return signature.build();
  }

  void call(const IndexMask mask,
            fn::MFParams params,
            fn::MFContext UNUSED(context)) const override
  {
    const VArray_Span<float2> sample_uvs = params.readonly_single_input<float2>(0, "Sample UV");
    MutableSpan<float3> r_translations = params.uninitialized_single_output_if_required<float3>(
        1, "Translation");
    MutableSpan<float3> r_rotations = params.uninitialized_single_output_if_required<float3>(
        2, "Rotation");

    const bool compute_translation = !r_translations.is_empty();
    const bool compute_rotation = !r_rotations.is_empty();

    for (const int i : mask) {
      const float2 &sample_uv = sample_uvs[i];
      const ReverseUVSampler::Result sample_result = reverse_uv_sampler->sample(sample_uv);
      if (sample_result.type != ReverseUVSampler::ResultType::Ok) {
        if (compute_translation) {
          r_translations[i] = float3(0.0f);
        }
        if (compute_rotation) {
          r_rotations[i] = float3(0.0f);
        }
        continue;
      }

      const MLoopTri &looptri = *sample_result.looptri;
      const float3 &bary_weights = sample_result.bary_weights;

      const int corner_0 = looptri.tri[0];
      const int corner_1 = looptri.tri[1];
      const int corner_2 = looptri.tri[2];

      const int vert_0 = mesh_->mloop[corner_0].v;
      const int vert_1 = mesh_->mloop[corner_1].v;
      const int vert_2 = mesh_->mloop[corner_2].v;

      const float3 &old_pos_0 = rest_positions_[corner_0];
      const float3 &old_pos_1 = rest_positions_[corner_1];
      const float3 &old_pos_2 = rest_positions_[corner_2];

      const float3 &new_pos_0 = verts_[vert_0].co;
      const float3 &new_pos_1 = verts_[vert_1].co;
      const float3 &new_pos_2 = verts_[vert_2].co;

      if (compute_translation) {
        const float3 old_pos = mix3(bary_weights, old_pos_0, old_pos_1, old_pos_2);
        const float3 new_pos = mix3(bary_weights, new_pos_0, new_pos_1, new_pos_2);
        const float3 translation = new_pos - old_pos;
        r_translations[i] = translation;
      }
      if (compute_rotation) {
        const float3 old_dir_1 = old_pos_1 - old_pos_0;
        const float3 old_dir_2 = old_pos_2 - old_pos_0;
        const float3 new_dir_1 = new_pos_1 - new_pos_0;
        const float3 new_dir_2 = new_pos_2 - new_pos_0;
        const float3 old_normal = math::normalize(math::cross(old_dir_1, old_dir_2));
        const float3 new_normal = math::normalize(math::cross(new_dir_1, new_dir_2));
        const float3 old_tangent_x = math::normalize(old_dir_1);
        const float3 new_tangent_x = math::normalize(new_dir_1);
        const float3 old_tangent_y = math::cross(old_normal, old_tangent_x);
        const float3 new_tangent_y = math::cross(new_normal, new_tangent_x);

        float3x3 old_transform;
        copy_v3_v3(old_transform.values[0], old_tangent_x);
        copy_v3_v3(old_transform.values[1], old_tangent_y);
        copy_v3_v3(old_transform.values[2], old_normal);

        float3x3 new_transform;
        copy_v3_v3(new_transform.values[0], new_tangent_x);
        copy_v3_v3(new_transform.values[1], new_tangent_y);
        copy_v3_v3(new_transform.values[2], new_normal);

        const float3x3 old_transform_inverse = old_transform.transposed();
        const float3x3 transform = new_transform * old_transform_inverse;

        float3 euler;
        mat3_to_eul(euler, transform.values);
        r_rotations[i] = euler;
      }
    }
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Mesh");
  Field<float3> rest_positions_field = params.extract_input<Field<float3>>("Rest Position");
  Field<float3> uv_map_field = params.extract_input<Field<float3>>("UV Map");
  Field<float3> sample_uv_field = params.extract_input<Field<float3>>("Sample UV");

  if (!geometry.has_mesh()) {
    params.set_default_remaining_outputs();
    return;
  }

  geometry.ensure_owns_direct_data();

  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();

  const MeshComponent &mesh_component = *geometry.get_component_for_read<MeshComponent>();
  GeometryComponentFieldContext field_context{mesh_component, ATTR_DOMAIN_CORNER};
  FieldEvaluator field_evaluator{field_context,
                                 mesh_component.attribute_domain_num(ATTR_DOMAIN_CORNER)};
  field_evaluator.add(rest_positions_field);
  field_evaluator.add(conversions.try_convert(std::move(uv_map_field), CPPType::get<float2>()));
  field_evaluator.evaluate();
  VArray<float3> rest_positions = field_evaluator.get_evaluated<float3>(0);
  VArray<float2> uv_map = field_evaluator.get_evaluated<float2>(1);

  auto fn = std::make_unique<SampleMeshDeformationFunction>(
      std::move(geometry), std::move(rest_positions), std::move(uv_map));

  Field<float2> sample_uv_field_float2 = conversions.try_convert(std::move(sample_uv_field),
                                                                 CPPType::get<float2>());
  auto operation = std::make_shared<FieldOperation>(
      std::move(fn), Vector<GField>{std::move(sample_uv_field_float2)});

  params.set_output("Translation", Field<float3>(operation, 0));
  params.set_output("Rotation", Field<float3>(operation, 1));

  params.set_default_remaining_outputs();
}

}  // namespace blender::nodes::node_geo_sample_mesh_deformation_cc

void register_node_type_geo_sample_mesh_deformation()
{
  namespace file_ns = blender::nodes::node_geo_sample_mesh_deformation_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_SAMPLE_MESH_DEFORMATION, "Sample Mesh Deformation", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.declare = file_ns::node_declare;
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  nodeRegisterType(&ntype);
}
