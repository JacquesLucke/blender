/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_float4x4.hh"
#include "BLI_index_mask_ops.hh"
#include "BLI_kdtree.h"
#include "BLI_noise.hh"
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
  const Depsgraph *depsgraph_ = nullptr;
  const Scene *scene_ = nullptr;
  ARegion *region_ = nullptr;
  const View3D *v3d_ = nullptr;
  const RegionView3D *rv3d_ = nullptr;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  VArray<float> point_factors_;
  Vector<int64_t> selected_curve_indices_;
  IndexMask curve_selection_;

  float4x4 curves_to_world_mat_;
  float4x4 world_to_curves_mat_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_re_;
  float brush_strength_;
  float clump_radius_re_;

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
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_re_ = BKE_brush_size_get(scene_, brush_);
    brush_strength_ = BKE_brush_alpha_get(scene_, brush_);
    clump_radius_re_ = brush_->curves_sculpt_settings->clump_radius;

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);
    if (curves_->curves_num() == 0) {
      return;
    }

    point_factors_ = get_point_selection(*curves_id_);
    curve_selection_ = retrieve_selected_curves(*curves_id_, selected_curve_indices_);

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

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : curve_selection_.slice(range)) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        bool &curve_changed = changed_curves[curve_i];
        for (const int point_i : points.drop_front(1)) {
          const float3 old_pos_cu = positions_cu[point_i];
          float2 old_pos_re;
          ED_view3d_project_float_v2_m4(region_, old_pos_cu, old_pos_re, projection.values);

          const float distance_to_brush_sq_re = math::distance_squared(old_pos_re, brush_pos_re_);
          if (distance_to_brush_sq_re > brush_radius_sq_re) {
            continue;
          }

          const float distance_to_brush_re = std::sqrt(distance_to_brush_sq_re);
          const float t = std::max(0.0f,
                                   safe_divide(distance_to_brush_re - clump_radius_re_,
                                               brush_radius_re_ - clump_radius_re_));
          const float radius_falloff = t * BKE_brush_curve_strength(brush_, t, 1.0f);
          const float tip_falloff = (point_i - points.first()) / (float)points.size();
          const float weight = brush_strength_ * radius_falloff * tip_falloff *
                               point_factors_[point_i];

          float3 pinch_center_wo;
          const float3 old_pos_wo = curves_to_world_mat_ * old_pos_cu;
          ED_view3d_win_to_3d(v3d_, region_, old_pos_wo, brush_pos_re_, pinch_center_wo);
          const float3 pinch_center_cu = world_to_curves_mat_ * pinch_center_wo;

          const float3 new_pos_cu = math::interpolate(old_pos_cu, pinch_center_cu, weight);
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
    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : curve_selection_.slice(range)) {
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
