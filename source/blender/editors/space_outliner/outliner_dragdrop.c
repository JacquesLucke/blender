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

/* ******************** Drop Target Find *********************** */

static TreeElement *outliner_dropzone_element(TreeElement *te, const float fmval[2], const bool children)
{
	if ((fmval[1] > te->ys) && (fmval[1] < (te->ys + UI_UNIT_Y))) {
		/* name and first icon */
		if ((fmval[0] > te->xs + UI_UNIT_X) && (fmval[0] < te->xend))
			return te;
	}
	/* Not it.  Let's look at its children. */
	if (children && (TREESTORE(te)->flag & TSE_CLOSED) == 0 && (te->subtree.first)) {
		for (te = te->subtree.first; te; te = te->next) {
			TreeElement *te_valid = outliner_dropzone_element(te, fmval, children);
			if (te_valid)
				return te_valid;
		}
	}
	return NULL;
}

/* Find tree element to drop into. */
static TreeElement *outliner_dropzone_find(const SpaceOops *soops, const float fmval[2], const bool children)
{
	TreeElement *te;

	for (te = soops->tree.first; te; te = te->next) {
		TreeElement *te_valid = outliner_dropzone_element(te, fmval, children);
		if (te_valid)
			return te_valid;
	}
	return NULL;
}

static TreeElement *outliner_drop_find(bContext *C, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	float fmval[2];
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	return outliner_dropzone_find(soops, fmval, true);
}

static ID *outliner_ID_drop_find(bContext *C, const wmEvent *event, short idcode)
{
	TreeElement *te = outliner_drop_find(C, event);
	TreeStoreElem *tselem = (te) ? TREESTORE(te) : NULL;

	if (te && te->idcode == idcode && tselem->type == 0) {
		return tselem->id;
	}
	else {
		return NULL;
	}
}

/* Find tree element to drop into, with additional before and after reorder support. */
static TreeElement *outliner_drop_insert_find(
        bContext *C, const wmEvent *event,
        TreeElementInsertType *r_insert_type)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	TreeElement *te_hovered;
	float view_mval[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &view_mval[0], &view_mval[1]);
	te_hovered = outliner_find_item_at_y(soops, &soops->tree, view_mval[1]);

	if (te_hovered) {
		/* mouse hovers an element (ignoring x-axis), now find out how to insert the dragged item exactly */
		const float margin = UI_UNIT_Y * (1.0f / 4);

		if (view_mval[1] < (te_hovered->ys + margin)) {
			if (TSELEM_OPEN(TREESTORE(te_hovered), soops)) {
				/* inserting after a open item means we insert into it, but as first child */
				if (BLI_listbase_is_empty(&te_hovered->subtree)) {
					*r_insert_type = TE_INSERT_INTO;
					return te_hovered;
				}
				else {
					*r_insert_type = TE_INSERT_BEFORE;
					return te_hovered->subtree.first;
				}
			}
			else {
				*r_insert_type = TE_INSERT_AFTER;
				return te_hovered;
			}
		}
		else if (view_mval[1] > (te_hovered->ys + (3 * margin))) {
			*r_insert_type = TE_INSERT_BEFORE;
			return te_hovered;
		}
		else {
			*r_insert_type = TE_INSERT_INTO;
			return te_hovered;
		}
	}
	else {
		/* mouse doesn't hover any item (ignoring x-axis), so it's either above list bounds or below. */
		TreeElement *first = soops->tree.first;
		TreeElement *last = soops->tree.last;

		if (view_mval[1] < last->ys) {
			*r_insert_type = TE_INSERT_AFTER;
			return last;
		}
		else if (view_mval[1] > (first->ys + UI_UNIT_Y)) {
			*r_insert_type = TE_INSERT_BEFORE;
			return first;
		}
		else {
			BLI_assert(0);
			return NULL;
		}
	}
}

static Collection *outliner_collection_from_tree_element_and_parents(TreeElement *te, TreeElement **r_te)
{
	while (te != NULL) {
		Collection *collection = outliner_collection_from_tree_element(te);
		if (collection) {
			*r_te = te;
			return collection;
		}
		te = te->parent;
	}
	return NULL;
}

static TreeElement *outliner_drop_insert_collection_find(
        bContext *C, const wmEvent *event,
        TreeElementInsertType *r_insert_type)
{
	TreeElement *te = outliner_drop_insert_find(C, event, r_insert_type);
	if (!te) return NULL;

	TreeElement *collection_te;
	Collection *collection = outliner_collection_from_tree_element_and_parents(te, &collection_te);
	if (!collection) return NULL;

	if (collection_te != te) {
		*r_insert_type = TE_INSERT_INTO;
	}

	/* We can't insert before/after master collection. */
	if (collection->flag & COLLECTION_IS_MASTER) {
		*r_insert_type = TE_INSERT_INTO;
	}

	return collection_te;
}

/* ******************** Parent Drop Operator *********************** */

static bool parent_drop_allowed(SpaceOops *soops, TreeElement *te, Object *potential_child)
{
	TreeStoreElem *tselem = TREESTORE(te);
	if (te->idcode != ID_OB || tselem->type != 0) {
		return false;
	}

	Object *potential_parent = (Object *)tselem->id;

	if (potential_parent == potential_child) return false;
	if (BKE_object_is_child_recursive(potential_child, potential_parent)) return false;
	if (potential_parent == potential_child->parent) return false;

	/* check that parent/child are both in the same scene */
	Scene *scene = (Scene *)outliner_search_back(soops, te, ID_SCE);

	/* currently outliner organized in a way that if there's no parent scene
		* element for object it means that all displayed objects belong to
		* active scene and parenting them is allowed (sergey)
		*/
	if (scene) {
		for (ViewLayer *view_layer = scene->view_layers.first;
		     view_layer;
		     view_layer = view_layer->next)
		{
			if (BKE_view_layer_base_find(view_layer, potential_child)) {
				return true;
			}
		}
		return false;
	}
	else {
		return true;
	}
}

static bool allow_parenting_without_modifier_key(SpaceOops *soops)
{
	switch (soops->outlinevis) {
		case SO_VIEW_LAYER:
			return soops->filter & SO_FILTER_NO_COLLECTION;
		case SO_SCENES:
			return true;
		default:
			return false;
	}
}
