/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_float4x4.hh"
#include "BLI_math_vector.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_context.h"
#include "BKE_paint.h"

#include "WM_api.h"

#include "ED_curves_sculpt.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "BKE_curves.hh"
#include "BKE_curves_sculpt.hh"

#include "RNA_access.h"

#include "DNA_curves_types.h"

#include "DEG_depsgraph.h"

#include "curves_sculpt_intern.h"
#include "paint_intern.h"

bool CURVES_SCULPT_mode_poll(struct bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && ob->mode & OB_MODE_SCULPT_CURVES;
}

namespace blender::ed::curves_sculpt {

using bke::CurvesGeometry;
using bke::CurvesSculptSession;
using bke::CurvesSculptStroke;

static bool stroke_get_location(bContext *C, float out[3], const float mouse[2])
{
  out[0] = mouse[0];
  out[1] = mouse[1];
  out[2] = 0;
  UNUSED_VARS(C);
  return true;
}

static bool stroke_test_start(bContext *C, struct wmOperator *op, const float mouse[2])
{
  UNUSED_VARS(C, op, mouse);
  return true;
}

static void stroke_update_step(bContext *C,
                               PaintStroke *UNUSED(paint_stroke),
                               PointerRNA *stroke_element)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *object = CTX_data_active_object(C);
  CurvesSculptSession &session = bke::curves_sculpt_session_ensure(*object);
  if (!session.current_stroke.has_value()) {
    session.current_stroke.emplace();
  }

  Curves &curves_id = *static_cast<Curves *>(object->data);
  CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);

  CurvesSculptStroke &stroke = *session.current_stroke;

  float2 mouse_position;
  RNA_float_get_array(stroke_element, "mouse", mouse_position);
  stroke.mouse_positions.append(mouse_position);

  ViewContext view_context;
  ED_view3d_viewcontext_init(C, &view_context, depsgraph);

  float3 ray_start, ray_end;
  ED_view3d_win_to_segment_clipped(
      depsgraph, view_context.region, view_context.v3d, mouse_position, ray_start, ray_end, true);
  const float3 ray_direction = math::normalize(ray_end - ray_start);
  std::cout << ray_start << " -> " << ray_end << "\n";

  curves.resize(curves.point_size + 2, curves.curve_size + 1);
  MutableSpan<float3> positions = curves.positions().take_back(2);
  positions[0] = ray_start + ray_direction * 4;
  positions[1] = ray_start + ray_direction * 6;
  MutableSpan<int> offsets = curves.offsets();
  offsets.last() = curves.point_size;

  DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
  ED_region_tag_redraw(view_context.region);
}

static void stroke_done(const bContext *C, PaintStroke *UNUSED(stroke))
{
  Object *object = CTX_data_active_object(C);
  CurvesSculptSession &session = bke::curves_sculpt_session_ensure(*object);
  session.current_stroke.reset();
}

static int sculpt_curves_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  PaintStroke *stroke = paint_stroke_new(C,
                                         op,
                                         stroke_get_location,
                                         stroke_test_start,
                                         stroke_update_step,
                                         nullptr,
                                         stroke_done,
                                         event->type);
  op->customdata = stroke;

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static void sculpt_curves_stroke_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op);
}

static void SCULPT_CURVES_OT_brush_stroke(struct wmOperatorType *ot)
{
  ot->name = "Stroke Curves Sculpt";
  ot->idname = "SCULPT_CURVES_OT_brush_stroke";
  ot->description = "Sculpt curves using a brush";

  ot->invoke = sculpt_curves_stroke_invoke;
  ot->modal = paint_stroke_modal;
  ot->cancel = sculpt_curves_stroke_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

}  // namespace blender::ed::curves_sculpt

void ED_operatortypes_sculpt_curves()
{
  using namespace blender::ed::curves_sculpt;
  WM_operatortype_append(SCULPT_CURVES_OT_brush_stroke);
}
