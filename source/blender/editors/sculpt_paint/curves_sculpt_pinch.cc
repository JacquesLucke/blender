/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_float4x4.hh"
#include "BLI_index_mask_ops.hh"
#include "BLI_kdtree.h"
#include "BLI_rand.hh"
#include "BLI_vector.hh"

#include "PIL_time.h"

#include "DEG_depsgraph.h"

#include "BKE_attribute_math.hh"
#include "BKE_brush.h"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_paint.h"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "UI_interface.h"

#include "WM_api.h"

/**
 * The code below uses a prefix naming convention to indicate the coordinate space:
 * cu: Local space of the curves object that is being edited.
 * su: Local space of the surface object.
 * wo: World space.
 * re: 2D coordinates within the region.
 */

namespace blender::ed::sculpt_paint {

class PinchOperation : public CurvesSculptStrokeOperation {
 private:
  Array<float> segment_lengths_cu_;

  friend struct PinchOperationExecutor;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

struct PinchOperationExecutor {
  PinchOperation *self_ = nullptr;
  Depsgraph *depsgraph_ = nullptr;
  Scene *scene_ = nullptr;
  Object *object_ = nullptr;
  ARegion *region_ = nullptr;
  View3D *v3d_ = nullptr;
  RegionView3D *rv3d_ = nullptr;

  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  float4x4 curves_to_world_mat_;
  float4x4 world_to_curves_mat_;

  CurvesSculpt *curves_sculpt_ = nullptr;
  Brush *brush_ = nullptr;
  float brush_radius_re_;
  float brush_strength_;

  float2 brush_pos_re_;

  void execute(PinchOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    self_ = &self;
    depsgraph_ = CTX_data_depsgraph_pointer(&C);
    scene_ = CTX_data_scene(&C);
    object_ = CTX_data_active_object(&C);
    region_ = CTX_wm_region(&C);
    v3d_ = CTX_wm_view3d(&C);
    rv3d_ = CTX_wm_region_view3d(&C);

    curves_sculpt_ = scene_->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush(&curves_sculpt_->paint);
    brush_radius_re_ = BKE_brush_size_get(scene_, brush_);
    brush_strength_ = BKE_brush_alpha_get(scene_, brush_);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);

    curves_to_world_mat_ = object_->obmat;
    world_to_curves_mat_ = curves_to_world_mat_.inverted();

    MutableSpan<float3> positions_cu = curves_->positions_for_write();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(rv3d_, object_, projection.values);

    Array<bool> changed_curves(curves_->curves_num(), false);

    const float brush_radius_sq_re = pow2f(brush_radius_re_);
    brush_pos_re_ = stroke_extension.mouse_position;

    if (stroke_extension.is_first) {
      this->initialize_segment_lengths();
    }

    threading::parallel_for(curves_->curves_range(), 256, [&](const IndexRange curves_range) {
      Vector<float2> curve_positions_re;

      for (const int curve_i : curves_range) {
        bool &curve_changed = changed_curves[curve_i];
        const IndexRange points = curves_->points_for_curve(curve_i);

        curve_positions_re.clear();
        for (const int point_i : points) {
          const float3 &pos_cu = positions_cu[point_i];
          float2 pos_re;
          ED_view3d_project_float_v2_m4(region_, pos_cu, pos_re, projection.values);
          curve_positions_re.append(pos_re);
        }

        float2 closest_to_brush_re;
        float closest_dist_to_brush_sq_re = FLT_MAX;
        // for (const float2 &pos_re : curve_positions_re) {
        //   const float dist_sq_re = math::distance_squared(pos_re, brush_pos_re_);
        //   if (dist_sq_re < closest_dist_to_brush_sq_re) {
        //     closest_dist_to_brush_sq_re = dist_sq_re;
        //     closest_to_brush_re = pos_re;
        //   }
        // }
        for (const int i_next : curve_positions_re.index_range().drop_front(1)) {
          const int i_prev = i_next - 1;
          const float2 &pos_prev_re = curve_positions_re[i_prev];
          const float2 &pos_next_re = curve_positions_re[i_next];
          float2 closest_on_segment_re;
          closest_to_line_segment_v2(
              closest_on_segment_re, brush_pos_re_, pos_prev_re, pos_next_re);
          const float dist_sq_re = math::distance_squared(closest_on_segment_re, brush_pos_re_);
          if (dist_sq_re < closest_dist_to_brush_sq_re) {
            closest_dist_to_brush_sq_re = dist_sq_re;
            closest_to_brush_re = closest_on_segment_re;
          }
        }
        if (closest_dist_to_brush_sq_re > brush_radius_sq_re) {
          continue;
        }
        const float2 move_direction = math::normalize(brush_pos_re_ - closest_to_brush_re);

        for (const int i : IndexRange(points.size()).drop_front(1)) {
          const int point_i = points[i];
          const float3 &old_pos_cu = positions_cu[point_i];
          const float2 &old_pos_re = curve_positions_re[i];

          const float distance_to_brush_sq_re = math::distance_squared(old_pos_re, brush_pos_re_);
          if (distance_to_brush_sq_re > brush_radius_sq_re) {
            continue;
          }

          const float distance_to_brush_re = std::sqrt(distance_to_brush_sq_re);
          const float radius_falloff = BKE_brush_curve_strength(
              brush_, distance_to_brush_re, brush_radius_re_);
          const float weight = brush_strength_ * radius_falloff;

          const float max_move_dist_re = math::distance(math::dot(move_direction, old_pos_re),
                                                        math::dot(move_direction, brush_pos_re_));
          const float move_dist_re = std::min(2.0f * weight, max_move_dist_re);
          const float2 move_diff_re = move_dist_re * move_direction;
          const float2 new_pos_re = old_pos_re + move_diff_re;

          float3 new_pos_wo;
          ED_view3d_win_to_3d(
              v3d_, region_, curves_to_world_mat_ * old_pos_cu, new_pos_re, new_pos_wo);
          const float3 new_pos_cu = world_to_curves_mat_ * new_pos_wo;
          positions_cu[point_i] = new_pos_cu;

          curve_changed = true;
        }
      }
    });

    this->restore_segment_lengths(changed_curves);
    curves_->tag_positions_changed();
    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(region_);
  }

  void initialize_segment_lengths()
  {
    const Span<float3> positions_cu = curves_->positions();
    self_->segment_lengths_cu_.reinitialize(curves_->points_num());
    threading::parallel_for(curves_->curves_range(), 128, [&](const IndexRange range) {
      for (const int curve_i : range) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        for (const int point_i : points.drop_back(1)) {
          const float3 &p1_cu = positions_cu[point_i];
          const float3 &p2_cu = positions_cu[point_i + 1];
          const float length_cu = math::distance(p1_cu, p2_cu);
          self_->segment_lengths_cu_[point_i] = length_cu;
        }
      }
    });
  }

  void restore_segment_lengths(const Span<bool> changed_curves)
  {
    const Span<float> expected_lengths_cu = self_->segment_lengths_cu_;
    MutableSpan<float3> positions_cu = curves_->positions_for_write();

    threading::parallel_for(changed_curves.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : range) {
        if (!changed_curves[curve_i]) {
          continue;
        }
        const IndexRange points = curves_->points_for_curve(curve_i);
        for (const int segment_i : IndexRange(points.size() - 1)) {
          const float3 &p1_cu = positions_cu[points[segment_i]];
          float3 &p2_cu = positions_cu[points[segment_i] + 1];
          const float3 direction = math::normalize(p2_cu - p1_cu);
          const float expected_length_cu = expected_lengths_cu[points[segment_i]];
          p2_cu = p1_cu + direction * expected_length_cu;
        }
      }
    });
  }
};

void PinchOperation::on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension)
{
  PinchOperationExecutor executor;
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_pinch_operation()
{
  return std::make_unique<PinchOperation>();
}

}  // namespace blender::ed::sculpt_paint
