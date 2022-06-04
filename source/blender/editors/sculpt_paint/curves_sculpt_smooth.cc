/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_brush.h"
#include "BKE_context.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"

#include "DNA_brush_types.h"

#include "WM_api.h"

#include "BLI_enumerable_thread_specific.hh"

#include "curves_sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

class SmoothOperation : public CurvesSculptStrokeOperation {
 private:
  /** Only used when a 3D brush is used. */
  CurvesBrush3D brush_3d_;

  friend struct SmoothOperationExecutor;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct SmoothOperationExecutor {
  SmoothOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  VArray<float> point_factors_;
  Vector<int64_t> selected_curve_indices_;
  IndexMask curve_selection_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_base_re_;
  float brush_radius_factor_;
  float brush_strength_;
  float2 brush_pos_re_;

  eBrushFalloffShape falloff_shape_;
  eBrushCurvesSculptSmoothMode smooth_mode_;

  float4x4 curves_to_world_mat_;
  float4x4 world_to_curves_mat_;

  SmoothOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(SmoothOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    UNUSED_VARS(C, stroke_extension);
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

    point_factors_ = get_point_selection(*curves_id_);
    curve_selection_ = retrieve_selected_curves(*curves_id_, selected_curve_indices_);

    curves_to_world_mat_ = object_->obmat;
    world_to_curves_mat_ = curves_to_world_mat_.inverted();

    falloff_shape_ = static_cast<eBrushFalloffShape>(brush_->falloff_shape);
    smooth_mode_ = static_cast<eBrushCurvesSculptSmoothMode>(
        brush_->curves_sculpt_settings->smooth_mode);

    if (stroke_extension.is_first) {
      if (falloff_shape_ == PAINT_FALLOFF_SHAPE_SPHERE) {
        self.brush_3d_ = *sample_curves_3d_brush(*ctx_.depsgraph,
                                                 *ctx_.region,
                                                 *ctx_.v3d,
                                                 *ctx_.rv3d,
                                                 *object_,
                                                 brush_pos_re_,
                                                 brush_radius_base_re_);
      }
    }

    if (falloff_shape_ == PAINT_FALLOFF_SHAPE_TUBE) {
      this->smooth_projected_with_symmetry();
    }
    else if (falloff_shape_ == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->smooth_spherical_with_symmetry();
    }
    else {
      BLI_assert_unreachable();
    }

    curves_->tag_positions_changed();
    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void smooth_projected_with_symmetry()
  {
    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->smooth_projected(brush_transform);
    }
  }

  void smooth_projected(const float4x4 &brush_transform)
  {
    switch (smooth_mode_) {
      case BRUSH_CURVES_SCULPT_SMOOTH_INDIVIDUAL: {
        this->smooth_projected_individual(brush_transform);
        break;
      }
      case BRUSH_CURVES_SCULPT_SMOOTH_DIRECTION: {
        this->smooth_projected_direction(brush_transform);
        break;
      }
    }
  }

  void smooth_projected_individual(const float4x4 &brush_transform)
  {
    const float4x4 brush_transform_inv = brush_transform.inverted();

    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      Vector<float2> old_curve_positions_re;
      for (const int curve_i : curve_selection_.slice(range)) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        old_curve_positions_re.clear();
        old_curve_positions_re.reserve(points.size());
        for (const int point_i : points) {
          const float3 &pos_cu = brush_transform_inv * positions_cu[point_i];
          float2 pos_re;
          ED_view3d_project_float_v2_m4(ctx_.region, pos_cu, pos_re, projection.values);
          old_curve_positions_re.append_unchecked(pos_re);
        }
        for (const int i : IndexRange(points.size()).drop_front(1).drop_back(1)) {
          const int point_i = points[i];
          const float2 &old_pos_re = old_curve_positions_re[i];
          const float dist_to_brush_sq_re = math::distance_squared(old_pos_re, brush_pos_re_);
          if (dist_to_brush_sq_re > brush_radius_sq_re) {
            continue;
          }

          const float dist_to_brush_re = std::sqrt(dist_to_brush_sq_re);
          const float radius_falloff = BKE_brush_curve_strength(
              brush_, dist_to_brush_re, brush_radius_re);
          const float weight = 0.1f * brush_strength_ * radius_falloff * point_factors_[point_i];

          const float2 &old_pos_prev_re = old_curve_positions_re[i - 1];
          const float2 &old_pos_next_re = old_curve_positions_re[i + 1];
          const float2 goal_pos_re = math::interpolate(old_pos_prev_re, old_pos_next_re, 0.5f);
          const float2 new_pos_re = math::interpolate(old_pos_re, goal_pos_re, weight);
          const float3 old_pos_cu = brush_transform_inv * positions_cu[point_i];
          float3 new_pos_wo;
          ED_view3d_win_to_3d(
              ctx_.v3d, ctx_.region, curves_to_world_mat_ * old_pos_cu, new_pos_re, new_pos_wo);
          const float3 new_pos_cu = brush_transform * (world_to_curves_mat_ * new_pos_wo);
          positions_cu[point_i] = new_pos_cu;
        }
      }
    });
  }

  void smooth_projected_direction(const float4x4 &brush_transform)
  {
    const float4x4 brush_transform_inv = brush_transform.inverted();

    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    const float2 direction_sum_re = threading::parallel_reduce(
        curve_selection_.index_range(),
        256,
        float2{0.0f, 0.0f},
        [&](const IndexRange range, float2 direction_sum_re) {
          for (const int curve_i : curve_selection_.slice(range)) {
            const IndexRange points = curves_->points_for_curve(curve_i);
            const float3 first_pos_cu = brush_transform_inv * positions_cu[points[0]];
            float2 prev_pos_re;
            ED_view3d_project_float_v2_m4(
                ctx_.region, first_pos_cu, prev_pos_re, projection.values);

            for (const int point_i : points.drop_front(1)) {
              const float3 pos_cu = brush_transform_inv * positions_cu[point_i];
              float2 pos_re;
              ED_view3d_project_float_v2_m4(ctx_.region, pos_cu, pos_re, projection.values);
              BLI_SCOPED_DEFER([&]() { prev_pos_re = pos_re; });

              const float dist_to_brush_sq_re = dist_squared_to_line_segment_v2(
                  brush_pos_re_, prev_pos_re, pos_re);
              if (dist_to_brush_sq_re > brush_radius_sq_re) {
                continue;
              }
              const float dist_to_brush_re = std::sqrt(dist_to_brush_sq_re);
              const float2 direction_re = pos_re - prev_pos_re;
              const float reduce_segment_length_factor = std::max<float>(
                  1.0f, 2.0f * brush_radius_re / math::length(direction_re));
              const float weight = reduce_segment_length_factor *
                                   (brush_radius_re - dist_to_brush_re) * point_factors_[point_i];
              direction_sum_re += direction_re * weight;
            }
          }
          return direction_sum_re;
        },
        [](const float2 &a, const float2 &b) { return a + b; });
    const float2 direction_re = math::normalize(direction_sum_re);

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : curve_selection_.slice(range)) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        bool found_align_pos = false;
        float2 align_pos_re;

        for (const int point_i : points.drop_front(1)) {
          const float3 old_pos_cu = brush_transform_inv * positions_cu[point_i];
          float2 old_pos_re;
          ED_view3d_project_float_v2_m4(ctx_.region, old_pos_cu, old_pos_re, projection.values);

          const float dist_to_brush_sq_re = math::distance_squared(old_pos_re, brush_pos_re_);
          if (dist_to_brush_sq_re > brush_radius_sq_re) {
            continue;
          }
          if (!found_align_pos) {
            const float3 align_pos_cu = brush_transform_inv * positions_cu[point_i - 1];
            ED_view3d_project_float_v2_m4(
                ctx_.region, align_pos_cu, align_pos_re, projection.values);
            found_align_pos = true;
          }
          const float dist_to_brush_re = std::sqrt(dist_to_brush_sq_re);
          const float radius_falloff = BKE_brush_curve_strength(
              brush_, dist_to_brush_re, brush_radius_re);
          const float weight = 0.1f * brush_strength_ * radius_falloff * point_factors_[point_i];

          float2 goal_pos_re;
          closest_to_line_v2(goal_pos_re, old_pos_re, align_pos_re, align_pos_re + direction_re);
          const float2 new_pos_re = math::interpolate(old_pos_re, goal_pos_re, weight);
          float3 new_pos_wo;
          ED_view3d_win_to_3d(
              ctx_.v3d, ctx_.region, curves_to_world_mat_ * old_pos_cu, new_pos_re, new_pos_wo);
          const float3 new_pos_cu = brush_transform * (world_to_curves_mat_ * new_pos_wo);
          positions_cu[point_i] = new_pos_cu;
        }
      }
    });
  }

  void smooth_spherical_with_symmetry()
  {
    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    float3 brush_pos_wo;
    ED_view3d_win_to_3d(ctx_.v3d,
                        ctx_.region,
                        curves_to_world_mat_ * self_->brush_3d_.position_cu,
                        brush_pos_re_,
                        brush_pos_wo);
    const float3 brush_pos_cu = world_to_curves_mat_ * brush_pos_wo;
    const float brush_radius_cu = self_->brush_3d_.radius_cu * brush_radius_factor_;

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->smooth_spherical(brush_transform * brush_pos_cu, brush_radius_cu);
    }
  }

  void smooth_spherical(const float3 &brush_pos_cu, const float brush_radius_cu)
  {
    switch (smooth_mode_) {
      case BRUSH_CURVES_SCULPT_SMOOTH_INDIVIDUAL: {
        this->smooth_spherical_individual(brush_pos_cu, brush_radius_cu);
        break;
      }
      case BRUSH_CURVES_SCULPT_SMOOTH_DIRECTION: {
        this->smooth_spherical_direction(brush_pos_cu, brush_radius_cu);
        break;
      }
    }
  }

  void smooth_spherical_individual(const float3 &brush_pos_cu, const float brush_radius_cu)
  {
    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      Vector<float3> old_curve_positions_cu;
      for (const int curve_i : curve_selection_.slice(range)) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        old_curve_positions_cu.clear();
        old_curve_positions_cu.extend(positions_cu.slice(points));
        for (const int i : IndexRange(points.size()).drop_front(1).drop_back(1)) {
          const int point_i = points[i];
          const float3 &old_pos_cu = old_curve_positions_cu[i];
          const float dist_to_brush_sq_cu = math::distance_squared(old_pos_cu, brush_pos_cu);
          if (dist_to_brush_sq_cu > brush_radius_sq_cu) {
            continue;
          }

          const float dist_to_brush_cu = std::sqrt(dist_to_brush_sq_cu);
          const float radius_falloff = BKE_brush_curve_strength(
              brush_, dist_to_brush_cu, brush_radius_cu);
          const float weight = 0.1f * brush_strength_ * radius_falloff * point_factors_[point_i];

          const float3 &old_pos_prev_cu = old_curve_positions_cu[i - 1];
          const float3 &old_pos_next_cu = old_curve_positions_cu[i + 1];
          const float3 goal_pos_cu = math::interpolate(old_pos_prev_cu, old_pos_next_cu, 0.5f);
          const float3 new_pos_cu = math::interpolate(old_pos_cu, goal_pos_cu, weight);
          positions_cu[point_i] = new_pos_cu;
        }
      }
    });
  }

  void smooth_spherical_direction(const float3 &brush_pos_cu, const float brush_radius_cu)
  {
    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    const float3 direction_sum_cu = threading::parallel_reduce(
        curve_selection_.index_range(),
        256,
        float3{0.0f, 0.0f, 0.0f},
        [&](const IndexRange range, float3 direction_sum_cu) {
          for (const int curve_i : curve_selection_.slice(range)) {
            const IndexRange points = curves_->points_for_curve(curve_i);
            for (const int point_i : points.drop_front(1)) {
              const float3 &pos_cu = positions_cu[point_i];
              const float3 &prev_pos_cu = positions_cu[point_i - 1];
              const float dist_to_brush_sq_cu = dist_squared_to_line_segment_v3(
                  brush_pos_cu, prev_pos_cu, pos_cu);
              const float dist_to_brush_cu = std::sqrt(dist_to_brush_sq_cu);
              const float3 direction_cu = pos_cu - prev_pos_cu;
              /* Make sure that very long segments don't change the results too much. */
              const float reduce_segment_length_factor = std::max<float>(
                  1.0f, 2.0f * brush_radius_cu / math::length(direction_cu));
              const float weight = reduce_segment_length_factor *
                                   (brush_radius_cu - dist_to_brush_cu) * point_factors_[point_i];
              direction_sum_cu += weight * direction_cu;
            }
          }
          return direction_sum_cu;
        },
        [](const float3 &a, const float3 &b) { return a + b; });
    const float3 direction_cu = math::normalize(direction_sum_cu);

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : curve_selection_.slice(range)) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        bool found_align_pos = false;
        float3 align_pos_cu;

        for (const int point_i : points.drop_front(1)) {
          const float3 old_pos_cu = positions_cu[point_i];
          const float dist_to_brush_sq_re = math::distance_squared(old_pos_cu, brush_pos_cu);
          if (dist_to_brush_sq_re > brush_radius_sq_cu) {
            continue;
          }
          if (!found_align_pos) {
            align_pos_cu = positions_cu[point_i - 1];
            found_align_pos = true;
          }
          const float dist_to_brush_cu = std::sqrt(dist_to_brush_sq_re);
          const float radius_falloff = BKE_brush_curve_strength(
              brush_, dist_to_brush_cu, brush_radius_cu);
          const float weight = 0.1f * brush_strength_ * radius_falloff * point_factors_[point_i];

          float3 goal_pos_cu;
          closest_to_line_v3(goal_pos_cu, old_pos_cu, align_pos_cu, align_pos_cu + direction_cu);
          const float3 new_pos_cu = math::interpolate(old_pos_cu, goal_pos_cu, weight);
          positions_cu[point_i] = new_pos_cu;
        }
      }
    });
  }
};

void SmoothOperation::on_stroke_extended(const bContext &C,
                                         const StrokeExtension &stroke_extension)
{
  SmoothOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_smooth_operation()
{
  return std::make_unique<SmoothOperation>();
}

}  // namespace blender::ed::sculpt_paint
