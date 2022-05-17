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
 public:
  void on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension) override;
};

struct PinchOperationExecutor {
  PinchOperation *self_ = nullptr;
  Depsgraph *depsgraph_ = nullptr;
  Scene *scene_ = nullptr;
  Object *object_ = nullptr;
  ARegion *region_ = nullptr;
  View3D *v3d_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  float4x4 curves_to_world_mat_;
  float4x4 world_to_curves_mat_;

  void execute(PinchOperation &self, bContext *C, const StrokeExtension &stroke_extension)
  {
    self_ = &self;
    depsgraph_ = CTX_data_depsgraph_pointer(C);
    scene_ = CTX_data_scene(C);
    object_ = CTX_data_active_object(C);
    region_ = CTX_wm_region(C);
    v3d_ = CTX_wm_view3d(C);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);

    curves_to_world_mat_ = object_->obmat;
    world_to_curves_mat_ = curves_to_world_mat_.inverted();

    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    for (const int i : curves_->points_range()) {
      positions_cu[i].x += 0.1f;
    }

    curves_->tag_positions_changed();
    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(region_);
  }
};

void PinchOperation::on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension)
{
  PinchOperationExecutor executor;
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_pinch_operation()
{
  return std::make_unique<PinchOperation>();
}

}  // namespace blender::ed::sculpt_paint
