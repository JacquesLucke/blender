/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_outliner/outliner_dragdrop.c
 *  \ingroup spoutliner
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_collection_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "outliner_intern.h"

static TreeElement *outliner_find_element_under_mouse(SpaceOops *soops, ARegion *ar, const wmEvent *event)
{
	const float mouse_y = UI_view2d_region_to_view_y(&ar->v2d, event->mval[1]);
	return outliner_find_item_at_y(soops, &soops->tree, mouse_y);
}

static TreeTraversalAction traverse_visit_insert_list(TreeElement *te, void *customdata)
{
	ListBase *selected_elements = (ListBase *)customdata;
	BLI_addtail(selected_elements, BLI_genericNodeN(te));
	return TRAVERSE_CONTINUE;
}

static ListBase *get_selected_elements(SpaceOops *soops)
{
	ListBase *elements = MEM_callocN(sizeof(ListBase), __func__);
	outliner_tree_traverse(soops, &soops->tree, 0, TSE_SELECTED, traverse_visit_insert_list, elements);
	return elements;
}

static int get_tree_element_id_type(TreeElement *te)
{
	TreeElementIcon data = tree_element_get_icon(TREESTORE(te), te);
	if (!data.drag_id) return -1;
	return GS(data.drag_id->name);
}

/* ************* Start Dragging ************** */

static int outliner_drag_init_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te = outliner_find_element_under_mouse(soops, ar, event);

	if (!te) {
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
	}

	/* Only drag element under mouse if it was not selected before. */
	if ((TREESTORE(te)->flag & TSE_SELECTED) == 0) {
		outliner_flag_set(&soops->tree, TSE_SELECTED, 0);
		TREESTORE(te)->flag |= TSE_SELECTED;
	}

	ListBase *elements = get_selected_elements(soops);
	WM_drag_start_tree_elements(C, elements);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_drag_init(wmOperatorType *ot)
{
	ot->name = "Initialize Drag and Drop";
	ot->idname = "OUTLINER_OT_drag_init";
	ot->description = "Drag element to another place";

	ot->invoke = outliner_drag_init_invoke;
	ot->poll = ED_operator_outliner_active;
}


wmDropTarget *outliner_drop_target_get(bContext *C, wmDragData *drag_data, const wmEvent *event)
{
	if (drag_data->type == DRAG_DATA_COLOR) {
		return WM_drop_target_new("OBJECT_OT_add", "Hello World", NULL);
	}
	return NULL;
}
