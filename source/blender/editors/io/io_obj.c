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

#include "io_obj.h"
#include "obj.h"

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
  struct OBJExportParams a;
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  a.print_name = RNA_boolean_get(op->ptr, "print_name");
  a.number = RNA_float_get(op->ptr, "print_the_float");

  bool ok = OBJ_export(C, filepath, &a);
  return ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void ui_obj_export_settings(uiLayout *layout, PointerRNA *imfptr)
{
  uiLayout *box;
  uiLayout *row;

  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Some Options"), ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "print_name", 0, NULL, ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "print_the_float", 0, NULL, ICON_NONE);
}

static void wm_obj_export_draw(bContext *UNUSED(C), wmOperator *op)
{
  PointerRNA ptr;
  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
  ui_obj_export_settings(op->layout, &ptr);
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

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_OBJECT_IO,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);

  RNA_def_boolean(ot->srna, "print_name", 0, "Print Name?", "If enabled, prints name of OP");
  RNA_def_float(ot->srna,
                "print_the_float",
                4.56,
                0.0f,
                10.0f,
                "Print the Float",
                "Prints the given Float",
                1.0f,
                9.0f);
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
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  struct OBJImportParams a;
  a.number = 3.0f;
  a.print_name = 1;
  bool ok = OBJ_import(C, filepath, &a);

  return ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
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
