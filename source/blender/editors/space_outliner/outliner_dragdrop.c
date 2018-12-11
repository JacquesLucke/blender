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
#include "UI_interface_icons.h"
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

static ID *get_id_from_tree_element(TreeElement *te)
{
	TreeElementIcon data = tree_element_get_icon(TREESTORE(te), te);
	return data.drag_id;
}

static bool has_selected_parent(TreeElement *te)
{
	for (TreeElement *te_parent = te->parent; te_parent; te_parent = te_parent->parent) {
		if (outliner_is_collection_tree_element(te_parent)) {
			if (TREESTORE(te_parent)->flag & TSE_SELECTED) {
				return true;
			}
		}
	}
	return false;
}

static Collection *find_parent_collection(bContext *C, TreeElement *te)
{
	for (TreeElement *te_parent = te->parent; te_parent; te_parent = te_parent->parent) {
		if (outliner_is_collection_tree_element(te_parent)) {
			return outliner_collection_from_tree_element(te_parent);
		}
	}
	return BKE_collection_master(CTX_data_scene(C));
}


/* ************* Start Dragging ************** */

static void init_drag_collection_children(bContext *C, ListBase *selected_tree_elements)
{
	ListBase *collection_children = MEM_callocN(sizeof(ListBase), __func__);

	LISTBASE_FOREACH (LinkData *, link, selected_tree_elements) {
		TreeElement *te = (TreeElement *)link->data;
		ID *id = get_id_from_tree_element(te);
		if (!id) continue;
		if (has_selected_parent(te)) continue;
		Collection *parent = find_parent_collection(C, te);

		wmDragCollectionChild *collection_child = MEM_callocN(sizeof(wmDragCollectionChild), __func__);
		collection_child->id = id;
		collection_child->parent = parent;

		BLI_addtail(collection_children, BLI_genericNodeN(collection_child));
	}

	WM_drag_start_collection_children(C, collection_children);
}

static void init_drag_single_id(bContext *C, ListBase *selected_tree_elements)
{
	if (BLI_listbase_is_single(selected_tree_elements)) {
		TreeElement *te = (TreeElement *)((LinkData *)selected_tree_elements->first)->data;
		ID *id = get_id_from_tree_element(te);
		if (id) {
			WM_drag_start_id(C, id);
			WM_drag_display_set_icon(WM_drag_get_active(C), UI_idcode_icon_get(GS(id->name)));
		}
	}
}

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

	if (soops->outlinevis == SO_VIEW_LAYER && (soops->filter & SO_FILTER_NO_COLLECTION) == 0) {
		init_drag_collection_children(C, elements);
	}
	else if (soops->outlinevis == SO_LIBRARIES) {
		init_drag_single_id(C, elements);
	}

	BLI_freelistN(elements);

	ED_area_tag_redraw(CTX_wm_area(C));

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


void outliner_drop_target_find(bContext *UNUSED(C), wmDropTargetFinder *UNUSED(finder), wmDragData *UNUSED(drag_data), const wmEvent *UNUSED(event))
{
}