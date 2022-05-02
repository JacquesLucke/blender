/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "curves_sculpt_intern.hh"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_mesh_runtime.h"

#include "DNA_brush_types.h"

/**
 * The code below uses a prefix naming convention to indicate the coordinate space:
 * cu: Local space of the curves object that is being edited.
 * su: Local space of the surface object.
 * wo: World space.
 * re: 2D coordinates within the region.
 */

namespace blender::ed::sculpt_paint {

class PuffOperation : public CurvesSculptStrokeOperation {
 private:
  float2 brush_pos_prev_re_;

  CurvesBrush3D brush_3d_;

  friend struct PuffOperationExecutor;

 public:
  void on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension) override;
};

struct PuffOperationExecutor {
  PuffOperation *self_ = nullptr;
  bContext *C_ = nullptr;
  Depsgraph *depsgraph_ = nullptr;
  Scene *scene_ = nullptr;
  Object *object_ = nullptr;
  ARegion *region_ = nullptr;
  View3D *v3d_ = nullptr;
  RegionView3D *rv3d_ = nullptr;

  CurvesSculpt *curves_sculpt_ = nullptr;
  Brush *brush_ = nullptr;
  float brush_radius_re_;
  float brush_strength_;

  eBrushFalloffShape falloff_shape_;

  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  const Object *surface_ob_ = nullptr;
  const Mesh *surface_ = nullptr;
  Span<MLoopTri> surface_looptris_;

  float4x4 curves_to_world_mat_;
  float4x4 world_to_curves_mat_;
  float4x4 surface_to_world_mat_;
  float4x4 world_to_surface_mat_;

  void execute(PuffOperation &self, bContext *C, const StrokeExtension &stroke_extension)
  {
    self_ = &self;

    BLI_SCOPED_DEFER([&]() { self_->brush_pos_prev_re_ = stroke_extension.mouse_position; });

    C_ = C;
    depsgraph_ = CTX_data_depsgraph_pointer(C);
    scene_ = CTX_data_scene(C);
    object_ = CTX_data_active_object(C);
    region_ = CTX_wm_region(C);
    v3d_ = CTX_wm_view3d(C);
    rv3d_ = CTX_wm_region_view3d(C);

    curves_sculpt_ = scene_->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush(&curves_sculpt_->paint);
    brush_radius_re_ = BKE_brush_size_get(scene_, brush_);
    brush_strength_ = BKE_brush_alpha_get(scene_, brush_);

    falloff_shape_ = static_cast<eBrushFalloffShape>(brush_->falloff_shape);

    curves_id_ = static_cast<Curves *>(object_->data);
    if (curves_id_->surface == nullptr || curves_id_->surface->type != OB_MESH) {
      return;
    }
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);
    if (curves_->curves_num() == 0) {
      return;
    }

    surface_ob_ = curves_id_->surface;
    surface_ = static_cast<const Mesh *>(surface_ob_->data);
    surface_looptris_ = {BKE_mesh_runtime_looptri_ensure(surface_),
                         BKE_mesh_runtime_looptri_len(surface_)};

    curves_to_world_mat_ = object_->obmat;
    world_to_curves_mat_ = curves_to_world_mat_.inverted();
    surface_to_world_mat_ = surface_ob_->obmat;
    world_to_surface_mat_ = surface_to_world_mat_.inverted();
  }
};

void PuffOperation::on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension)
{
  PuffOperationExecutor executor;
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_puff_operation()
{
  return std::make_unique<PuffOperation>();
}

}  // namespace blender::ed::sculpt_paint
