/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_brush.h"
#include "BKE_context.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"

#include "BLI_index_mask_ops.hh"
#include "BLI_kdtree.h"

#include "DNA_brush_types.h"

#include "WM_api.h"

#include "curves_sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

class DensityAddOperation : public CurvesSculptStrokeOperation {
 private:
  friend struct DensityAddOperationExecutor;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

struct DensityAddOperationExecutor {
  DensityAddOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  DensityAddOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(DensityAddOperation &self,
               const bContext &C,
               const StrokeExtension &stroke_extension)
  {
    UNUSED_VARS(self, C, stroke_extension);
  }
};

void DensityAddOperation::on_stroke_extended(const bContext &C,
                                             const StrokeExtension &stroke_extension)
{
  DensityAddOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

class DensitySubtractOperation : public CurvesSculptStrokeOperation {
 private:
  /** Only used when a 3D brush is used. */
  CurvesBrush3D brush_3d_;

  friend struct DensitySubtractOperationExecutor;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct DensitySubtractOperationExecutor {
  DensitySubtractOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  Vector<int64_t> selected_curve_indices_;
  IndexMask curve_selection_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_base_re_;
  float brush_radius_factor_;
  float brush_strength_;
  float2 brush_pos_re_;

  float minimum_distance_;

  eBrushFalloffShape falloff_shape_;

  float4x4 curves_to_world_mat_;
  float4x4 world_to_curves_mat_;

  KDTree_3d *root_points_kdtree_;

  DensitySubtractOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(DensitySubtractOperation &self,
               const bContext &C,
               const StrokeExtension &stroke_extension)
  {
    self_ = &self;

    object_ = CTX_data_active_object(&C);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);
    if (curves_->curves_num() == 0) {
      return;
    }

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_base_re_ = BKE_brush_size_get(ctx_.scene, brush_);
    brush_radius_factor_ = brush_radius_factor(*brush_, stroke_extension);
    brush_strength_ = brush_strength_get(*ctx_.scene, *brush_, stroke_extension);
    brush_pos_re_ = stroke_extension.mouse_position;

    minimum_distance_ = brush_->curves_sculpt_settings->minimum_distance;

    curve_selection_ = retrieve_selected_curves(*curves_id_, selected_curve_indices_);

    curves_to_world_mat_ = object_->obmat;
    world_to_curves_mat_ = curves_to_world_mat_.inverted();

    falloff_shape_ = static_cast<eBrushFalloffShape>(brush_->falloff_shape);

    const Span<float3> positions_cu = curves_->positions();

    root_points_kdtree_ = BLI_kdtree_3d_new(curve_selection_.size());
    BLI_SCOPED_DEFER([&]() { BLI_kdtree_3d_free(root_points_kdtree_); });
    for (const int curve_i : curve_selection_) {
      const int first_point_i = curves_->offsets()[curve_i];
      const float3 &pos_cu = positions_cu[first_point_i];
      BLI_kdtree_3d_insert(root_points_kdtree_, curve_i, pos_cu);
    }
    BLI_kdtree_3d_balance(root_points_kdtree_);

    Array<bool> curves_to_delete(curves_->curves_num(), false);
    this->reduce_density_projected_with_symmetry(curves_to_delete);

    Vector<int64_t> indices;
    const IndexMask mask = index_mask_ops::find_indices_based_on_predicate(
        curves_->curves_range(), 4096, indices, [&](const int curve_i) {
          return curves_to_delete[curve_i];
        });

    curves_->remove_curves(mask);

    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void reduce_density_projected_with_symmetry(MutableSpan<bool> curves_to_delete)
  {
    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->reduce_density_projected(brush_transform, curves_to_delete);
    }
  }

  void reduce_density_projected(const float4x4 &brush_transform,
                                MutableSpan<bool> curves_to_delete)
  {
    const Span<float3> positions_cu = curves_->positions();
    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    for (const int curve_i : curve_selection_) {
      if (curves_to_delete[curve_i]) {
        continue;
      }
      const int first_point_i = curves_->offsets()[curve_i];
      const float3 orig_pos_cu = positions_cu[first_point_i];
      const float3 pos_cu = brush_transform * orig_pos_cu;
      float2 pos_re;
      ED_view3d_project_float_v2_m4(ctx_.region, pos_cu, pos_re, projection.values);
      const float dist_to_brush_sq_re = math::distance_squared(brush_pos_re_, pos_re);
      if (dist_to_brush_sq_re > brush_radius_sq_re) {
        continue;
      }
      const float dist_to_brush_re = std::sqrt(dist_to_brush_sq_re);
      const float radius_falloff = BKE_brush_curve_strength(
          brush_, dist_to_brush_re, brush_radius_re);
      const float distance_to_check = radius_falloff * minimum_distance_ * brush_strength_;
      BLI_kdtree_3d_range_search_cb_cpp(
          root_points_kdtree_,
          orig_pos_cu,
          distance_to_check,
          [&](const int other_curve_i, const float *UNUSED(co), float UNUSED(dist_sq)) {
            if (other_curve_i == curve_i) {
              return true;
            }
            curves_to_delete[other_curve_i] = true;
            return true;
          });
    }
  }
};

void DensitySubtractOperation::on_stroke_extended(const bContext &C,
                                                  const StrokeExtension &stroke_extension)
{
  DensitySubtractOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_density_operation(
    const BrushStrokeMode brush_mode, const bContext &C)
{
  const Scene &scene = *CTX_data_scene(&C);
  const Brush &brush = *BKE_paint_brush_for_read(&scene.toolsettings->curves_sculpt->paint);

  const bool use_add = (brush_mode == BRUSH_STROKE_INVERT) == ((brush.flag & BRUSH_DIR_IN) != 0);
  if (use_add) {
    return std::make_unique<DensityAddOperation>();
  }
  return std::make_unique<DensitySubtractOperation>();
}

}  // namespace blender::ed::sculpt_paint
