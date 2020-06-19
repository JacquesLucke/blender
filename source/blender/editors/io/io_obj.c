/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editor/io
 */

#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"

#include "IO_wavefront_obj.h"
#include "io_obj.h"

const EnumPropertyItem io_obj_transform_axis_forward[] = {
    {OBJ_AXIS_X_FORWARD, "X_FORWARD", 0, "X", "Positive X-axis"},
    {OBJ_AXIS_Y_FORWARD, "Y_FORWARD", 0, "Y", "Positive Y-axis"},
    {OBJ_AXIS_Z_FORWARD, "Z_FORWARD", 0, "Z", "Positive Z-axis"},
    {OBJ_AXIS_NEGATIVE_X_FORWARD, "NEGATIVE_X_FORWARD", 0, "-X", "Negative X-axis"},
    {OBJ_AXIS_NEGATIVE_Y_FORWARD, "NEGATIVE_Y_FORWARD", 0, "-Y (Default)", "Negative Y-axis"},
    {OBJ_AXIS_NEGATIVE_Z_FORWARD, "NEGATIVE_Z_FORWARD", 0, "-Z", "Negative Z-axis"},
    {0, NULL, 0, NULL, NULL}};

const EnumPropertyItem io_obj_transform_axis_up[] = {
    {OBJ_AXIS_X_UP, "X_UP", 0, "X", "Positive X-axis"},
    {OBJ_AXIS_Y_UP, "Y_UP", 0, "Y", "Positive Y-axis"},
    {OBJ_AXIS_Z_UP, "Z_UP", 0, "Z (Default)", "Positive Z-axis"},
    {OBJ_AXIS_NEGATIVE_X_UP, "NEGATIVE_X_UP", 0, "-X", "Negative X-axis"},
    {OBJ_AXIS_NEGATIVE_Y_UP, "NEGATIVE_Y_UP", 0, "-Y", "Negative Y-axis"},
    {OBJ_AXIS_NEGATIVE_Z_UP, "NEGATIVE_Z_UP", 0, "-Z", "Negative Z-axis"},
    {0, NULL, 0, NULL, NULL}};

static int wm_obj_export_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{

  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    Main *bmain = CTX_data_main(C);
    char filepath[FILE_MAX];

    if (BKE_main_blendfile_path(bmain)[0] == '\0') {
      BLI_strncpy(filepath, "untitled", sizeof(filepath));
    }
    else {
      BLI_strncpy(filepath, BKE_main_blendfile_path(bmain), sizeof(filepath));
    }

    BLI_path_extension_replace(filepath, sizeof(filepath), ".obj");
    RNA_string_set(op->ptr, "filepath", filepath);
  }

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;

  UNUSED_VARS(event);
}

static int wm_obj_export_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }
  struct OBJExportParams export_params;
  RNA_string_get(op->ptr, "filepath", export_params.filepath);
  export_params.export_animation = RNA_boolean_get(op->ptr, "export_animation");
  export_params.start_frame = RNA_int_get(op->ptr, "start_frame");
  export_params.end_frame = RNA_int_get(op->ptr, "end_frame");

  export_params.forward_axis = RNA_enum_get(op->ptr, "forward_axis");
  export_params.up_axis = RNA_enum_get(op->ptr, "up_axis");
  export_params.scaling_factor = RNA_float_get(op->ptr, "scaling_factor");

  export_params.export_uv = RNA_boolean_get(op->ptr, "export_uv");
  export_params.export_normals = RNA_boolean_get(op->ptr, "export_normals");
  export_params.export_triangulated_mesh = RNA_boolean_get(op->ptr, "export_triangulated_mesh");
  export_params.export_curves_as_nurbs = RNA_boolean_get(op->ptr, "export_curves_as_nurbs");

  OBJ_export(C, &export_params);

  return OPERATOR_FINISHED;
}

static void ui_obj_export_settings(uiLayout *layout, PointerRNA *imfptr)
{
  uiLayout *box;
  uiLayout *row;
  bool export_animation = RNA_boolean_get(imfptr, "export_animation");

  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  /* Animation options. */
  uiItemL(row, IFACE_("Animation"), ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "export_animation", 0, NULL, ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "start_frame", 0, NULL, ICON_NONE);
  uiLayoutSetEnabled(row, export_animation);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "end_frame", 0, NULL, ICON_NONE);
  uiLayoutSetEnabled(row, export_animation);

  /* Geometry Transform options. */
  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Geometry Transform"), ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "forward_axis", 0, NULL, ICON_NONE);

  row = uiLayoutRow(box, 1);
  uiItemR(row, imfptr, "up_axis", 0, NULL, ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "scaling_factor", 0, NULL, ICON_NONE);

  /* File write options. */
  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("File Write Options"), ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "export_uv", 0, NULL, ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "export_normals", 0, NULL, ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "export_triangulated_mesh", 0, NULL, ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "export_curves_as_nurbs", 0, NULL, ICON_NONE);
}

static void wm_obj_export_draw(bContext *UNUSED(C), wmOperator *op)
{
  PointerRNA ptr;
  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
  ui_obj_export_settings(op->layout, &ptr);
}

static bool wm_obj_export_check(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  Scene *scene = CTX_data_scene(C);
  bool ret = false;
  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_path_extension_check(filepath, ".obj")) {
    BLI_path_extension_ensure(filepath, FILE_MAX, ".obj");
    RNA_string_set(op->ptr, "filepath", filepath);
    ret = true;
  }

  /* Set the default export frames to the current one in viewport. */
  if (RNA_int_get(op->ptr, "start_frame") == INT_MAX) {
    RNA_int_set(op->ptr, "start_frame", CFRA);
    RNA_int_set(op->ptr, "end_frame", CFRA);
    ret = true;
  }

  /* End frame should be greater than or equal to start frame. */
  if (RNA_int_get(op->ptr, "start_frame") > RNA_int_get(op->ptr, "end_frame")) {
    RNA_int_set(op->ptr, "end_frame", RNA_int_get(op->ptr, "start_frame"));
    ret = true;
  }

  /* Both forward and up axes cannot be the same (or same except opposite sign). */
  if ((RNA_enum_get(op->ptr, "forward_axis")) % 3 == (RNA_enum_get(op->ptr, "up_axis")) % 3) {
    /* TODO (ankitm) Show a warning here. */
    RNA_enum_set(op->ptr, "up_axis", RNA_enum_get(op->ptr, "up_axis") % 3 + 1);
    ret = true;
  }
  return ret;
}

void WM_OT_obj_export(struct wmOperatorType *ot)
{
  ot->name = "Export Wavefront OBJ";
  ot->description = "Save the scene to a Wavefront OBJ file";
  ot->idname = "WM_OT_obj_export";

  ot->invoke = wm_obj_export_invoke;
  ot->exec = wm_obj_export_exec;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_obj_export_draw;
  ot->check = wm_obj_export_check;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_OBJECT_IO,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);

  RNA_def_boolean(ot->srna,
                  "export_animation",
                  false,
                  "Export Animation",
                  "Write selected range of frames to individual files. If unchecked, exports the "
                  "current viewport frame");
  RNA_def_int(ot->srna,
              "start_frame",
              INT_MAX,
              -INT_MAX,
              INT_MAX,
              "Start Frame",
              "The first frame to be exported",
              0,
              250);
  RNA_def_int(ot->srna,
              "end_frame",
              1,
              -INT_MAX,
              INT_MAX,
              "End Frame",
              "The last frame to be exported",
              0,
              250);
  RNA_def_enum(ot->srna,
               "forward_axis",
               io_obj_transform_axis_forward,
               OBJ_AXIS_NEGATIVE_Y_FORWARD,
               "Forward",
               "");
  RNA_def_enum(ot->srna, "up_axis", io_obj_transform_axis_up, OBJ_AXIS_Z_UP, "Up", "");
  RNA_def_float(ot->srna,
                "scaling_factor",
                1.000f,
                0.001f,
                10 * 1000.000f,
                "Scale",
                "Scaling Factor: both position and object size are affected",
                0.01,
                1000.000f);
  RNA_def_boolean(ot->srna, "export_uv", true, "Export UVs", "Export UV coordinates");
  RNA_def_boolean(
      ot->srna, "export_normals", true, "Export normals", "Export per face per vertex normals");
  RNA_def_boolean(
      ot->srna,
      "export_triangulated_mesh",
      false,
      "Export Triangulated Mesh",
      "The mesh in viewport will not be affected. Behaves the same as Triangulate Modifier with "
      "ngon-method: \"Beauty\", quad-method: \"Shortest Diagonal\", min vertices: 4");
  RNA_def_boolean(ot->srna,
                  "export_curves_as_nurbs",
                  false,
                  "Export curves as NURBS",
                  "If false, writes the curve as a mesh withouth modifying the scene");
}

static int wm_obj_import_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
  UNUSED_VARS(event);
}
static int wm_obj_import_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }
  /* Import functions and structs are incomplete now. Only dummy functions are written. */
  struct OBJImportParams import_params;
  RNA_string_get(op->ptr, "filepath", import_params.filepath);
  OBJ_import(C, &import_params);

  return OPERATOR_FINISHED;
}
static void wm_obj_import_draw(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
}

void WM_OT_obj_import(struct wmOperatorType *ot)
{
  ot->name = "Import Wavefront OBJ";
  ot->description = "Load an Wavefront OBJ scene";
  ot->idname = "WM_OT_obj_import";

  ot->invoke = wm_obj_import_invoke;
  ot->exec = wm_obj_import_exec;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_obj_import_draw;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_OBJECT_IO,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);
}
