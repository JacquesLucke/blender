/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_float4x4.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_context.h"
#include "BKE_paint.h"

#include "WM_api.h"
#include "WM_toolsystem.h"

#include "ED_curves_sculpt.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "BKE_curves.hh"
#include "BKE_curves_sculpt.hh"

#include "RNA_access.h"

#include "DNA_brush_types.h"
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
  ToolSettings *tool_settings = CTX_data_tool_settings(C);
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

  float4x4 ob_imat;
  invert_m4_m4(ob_imat.values, object->obmat);

  Brush *brush = BKE_paint_brush(&tool_settings->curves_sculpt->paint);

  float3 ray_start, ray_end;
  ED_view3d_win_to_segment_clipped(
      depsgraph, view_context.region, view_context.v3d, mouse_position, ray_start, ray_end, true);
  ray_start = ob_imat * ray_start;
  ray_end = ob_imat * ray_end;
  stroke.ray_starts.append(ray_start);
  stroke.ray_ends.append(ray_end);
  const float3 ray_direction = math::normalize(ray_end - ray_start);
  // std::cout << ray_start << " -> " << ray_end << "\n";

  if (stroke.ray_starts.size() == 1) {
    return;
  }

  if (brush->curves_sculpt_tool == 0) {
    const float3 P1 = stroke.ray_starts.as_span().take_back(2)[0];
    const float3 P2 = stroke.ray_starts.as_span().take_back(2)[1];
    const float3 P3 = stroke.ray_ends.as_span().take_back(2)[0];
    const float3 P4 = stroke.ray_ends.as_span().take_back(2)[1];

    Vector<int> curves_to_remove;
    MutableSpan<float3> positions = curves.positions();
    for (const int curve_i : IndexRange(curves.curve_size)) {
      const IndexRange point_range = curves.range_for_curve(curve_i);
      for (const int segment_i : IndexRange(point_range.size() - 1)) {
        const float3 start = positions[point_range[segment_i]];
        const float3 end = positions[point_range[segment_i + 1]];
        float lambda;
        const bool is_intersecting =
            isect_line_segment_tri_v3(start, end, P1, P2, P3, &lambda, nullptr) ||
            isect_line_segment_tri_v3(start, end, P2, P3, P4, &lambda, nullptr);
        if (is_intersecting) {
          curves_to_remove.append(curve_i);
          break;
        }
      }
    }
    for (const int curve_i : curves_to_remove) {
      for (const int point_i : curves.range_for_curve(curve_i)) {
        positions[point_i] = {0.0f, 0.0f, 0.0f};
      }
    }
    curves.tag_positions_changed();
  }
  else {
    curves.resize(curves.point_size + 2, curves.curve_size + 1);
    MutableSpan<float3> positions = curves.positions().take_back(2);
    positions[0] = ray_start + ray_direction * 8;
    positions[1] = ray_start + ray_direction * 10;
    MutableSpan<int> offsets = curves.offsets();
    offsets.last() = curves.point_size;
  }

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

static bool curves_sculptmode_toggle_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return false;
  }
  if (ob->type != OB_CURVES) {
    return false;
  }
  return true;
}

static int curves_sculptmode_toggle_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  const bool is_mode_set = ob->mode == OB_MODE_SCULPT_CURVES;

  if (is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, OB_MODE_SCULPT_CURVES, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (is_mode_set) {
    ob->mode = OB_MODE_OBJECT;
    WM_paint_cursor_end(
        static_cast<wmPaintCursor *>(scene->toolsettings->curves_sculpt->paint.paint_cursor));
    scene->toolsettings->curves_sculpt->paint.paint_cursor = nullptr;
  }
  else {
    BKE_paint_ensure(scene->toolsettings, (Paint **)&scene->toolsettings->curves_sculpt);
    ob->mode = OB_MODE_SCULPT_CURVES;

    paint_cursor_start(&scene->toolsettings->curves_sculpt->paint, nullptr);
  }

  WM_toolsystem_update_from_context_view3d(C);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);
  return OPERATOR_CANCELLED;
}

static void CURVES_OT_sculptmode_toggle(wmOperatorType *ot)
{
  ot->name = "Curve Sculpt Mode Toggle";
  ot->idname = "CURVES_OT_sculptmode_toggle";
  ot->description = "Enter/Exit sculpt mode for curves";

  ot->exec = curves_sculptmode_toggle_exec;
  ot->poll = curves_sculptmode_toggle_poll;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

}  // namespace blender::ed::curves_sculpt

void ED_operatortypes_sculpt_curves()
{
  using namespace blender::ed::curves_sculpt;
  WM_operatortype_append(SCULPT_CURVES_OT_brush_stroke);
  WM_operatortype_append(CURVES_OT_sculptmode_toggle);
}
