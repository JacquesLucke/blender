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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_dragdrop.c
 *  \ingroup wm
 *
 * Our own drag-and-drop, drag state and drop boxes.
 */

#include <string.h>

#include "DNA_windowmanager_types.h"
#include "DNA_screen_types.h"
#include "DNA_collection_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_context.h"
#include "BKE_idcode.h"
#include "BKE_screen.h"

#include "GPU_shader.h"

#include "IMB_imbuf_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "ED_outliner.h"
#include "ED_fileselect.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"

/* ********************* Free Data ********************* */

static void drag_data_free_filepaths(wmDragData *drag_data)
{
	for (int i = 0; i < drag_data->data.filepaths.amount; i++) {
		MEM_freeN(drag_data->data.filepaths.paths[i]);
	}
	MEM_freeN(drag_data->data.filepaths.paths);
}

static void drag_data_free_collection_children(wmDragData *drag_data)
{
	ListBase *list = drag_data->data.collection_children;
	LISTBASE_FOREACH (LinkData *, link, list) {
		MEM_freeN(link->data);
	}
	BLI_freelistN(list);
	MEM_freeN(list);
}

void WM_drag_data_free(wmDragData *drag_data)
{
	switch (drag_data->type) {
		case DRAG_DATA_FILEPATHS:
			drag_data_free_filepaths(drag_data);
			break;
		case DRAG_DATA_COLLECTION_CHILDREN:
			drag_data_free_collection_children(drag_data);
			break;
		default:
			break;
	}

	MEM_freeN(drag_data);
}

void WM_drop_target_free(wmDropTarget *drop_target)
{
	if (drop_target->free_idname) {
		MEM_freeN(drop_target->ot_idname);
	}
	if (drop_target->free_tooltip) {
		MEM_freeN(drop_target->tooltip);
	}
	if (drop_target->free) {
		MEM_freeN(drop_target);
	}
}

void WM_drag_stop(wmWindowManager *wm)
{
	if (wm->drag.data) {
		WM_drag_data_free(wm->drag.data);
	}
	if (wm->drag.target) {
		WM_drop_target_free(wm->drag.target);
	}
	wm->drag.data = NULL;
	wm->drag.target = NULL;
}

/* ********************* Start Dragging ********************* */

static void start_dragging_data(struct bContext *C, wmDragData *drag_data)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	WM_drag_stop(wm);
	wm->drag.data = drag_data;
	wm->drag.target = NULL;
}

static wmDragData *WM_drag_data_new(void) {
	return MEM_callocN(sizeof(wmDragData), "drag data");
}

wmDragData *WM_drag_start_id(struct bContext *C, ID *id)
{
	wmDragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_ID;
	drag_data->data.id = id;

	start_dragging_data(C, drag_data);
	return drag_data;
}

wmDragData *WM_drag_start_filepaths(struct bContext *C, const char **filepaths, int amount)
{
	BLI_assert(amount > 0);

	char **paths = MEM_malloc_arrayN(amount, sizeof(char *), __func__);
	for (int i = 0; i < amount; i++) {
		paths[i] = BLI_strdup(filepaths[i]);
	}

	wmDragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_FILEPATHS;
	drag_data->data.filepaths.amount = amount;
	drag_data->data.filepaths.paths = paths;

	start_dragging_data(C, drag_data);
	return drag_data;
}

wmDragData *WM_drag_start_filepath(struct bContext *C, const char *filepath)
{
	return WM_drag_start_filepaths(C, &filepath, 1);
}

wmDragData *WM_drag_start_color(struct bContext *C, float color[3], bool gamma_corrected)
{
	wmDragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_COLOR;
	copy_v3_v3(drag_data->data.color.color, color);
	drag_data->data.color.gamma_corrected = gamma_corrected;

	start_dragging_data(C, drag_data);
	return drag_data;
}

wmDragData *WM_drag_start_value(struct bContext *C, double value)
{
	wmDragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_VALUE;
	drag_data->data.value = value;

	start_dragging_data(C, drag_data);
	return drag_data;
}

wmDragData *WM_drag_start_rna(struct bContext *C, struct PointerRNA *rna)
{
	wmDragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_RNA;
	drag_data->data.rna = rna;

	start_dragging_data(C, drag_data);
	return drag_data;
}

wmDragData *WM_drag_start_name(struct bContext *C, const char *name)
{
	wmDragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_NAME;
	drag_data->data.name = BLI_strdup(name);

	start_dragging_data(C, drag_data);
	return drag_data;
}

wmDragData *WM_drag_start_collection_children(struct bContext *C, ListBase *collection_children)
{
	wmDragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_COLLECTION_CHILDREN;
	drag_data->data.collection_children = collection_children;

	start_dragging_data(C, drag_data);
	return drag_data;
}


/* ********************* Set Display Options ********************* */

void WM_drag_display_set_image(
        wmDragData *drag_data, ImBuf *imb,
        float scale, int width, int height)
{
	drag_data->display_type = DRAG_DISPLAY_IMAGE;
	drag_data->display.image.imb = imb;
	drag_data->display.image.scale = scale;
	drag_data->display.image.width = width;
	drag_data->display.image.height = height;
}

void WM_drag_display_set_icon(wmDragData *drag_data, int icon_id)
{
	drag_data->display_type = DRAG_DISPLAY_ICON;
	drag_data->display.icon_id = icon_id;
}

void WM_drag_display_set_color(wmDragData *drag_data, float color[3])
{
	drag_data->display_type = DRAG_DISPLAY_COLOR;
	copy_v3_v3(drag_data->display.color, color);
}

void WM_drag_display_set_color_derived(wmDragData *drag_data)
{
	BLI_assert(drag_data->type == DRAG_DATA_COLOR);
	WM_drag_display_set_color(drag_data, drag_data->data.color.color);
}


/* ********************* Drop Target Creation ********************* */

void WM_drop_target_propose(wmDropTargetFinder *finder, wmDropTarget *target)
{
	if (target == NULL) {
		return;
	}
	else if (finder->current == NULL) {
		finder->current = target;
	}
	else if (target->size < finder->current->size) {
		WM_drop_target_free(finder->current);
		finder->current = target;
	}
	else {
		WM_drop_target_free(target);
	}
}

static enum DropTargetSize drop_target_get_current_size(wmDropTargetFinder *finder)
{
	if (finder->current) return finder->current->size;
	else return DROP_TARGET_SIZE_MAX;
}

void WM_drop_target_propose__template_1(
        wmDropTargetFinder *finder,
        enum DropTargetSize size, const char *ot_idname,
        const char *tooltip, wmDropTargetSetProps set_properties)
{
	WM_drop_target_propose__template_2(finder, size, ot_idname, tooltip, set_properties, WM_OP_INVOKE_DEFAULT);
}

void WM_drop_target_propose__template_2(
        wmDropTargetFinder *finder,
        enum DropTargetSize size, const char *ot_idname,
        const char *tooltip, wmDropTargetSetProps set_properties,
        short context)
{
	if (size >= drop_target_get_current_size(finder)) return;
	WM_drop_target_propose(finder, WM_drop_target_new(
	        size, (char *)ot_idname, (char *)tooltip, set_properties,
	        context, true, false, false));
}

wmDropTarget *WM_drop_target_new(
        enum DropTargetSize size, char *ot_idname, char *tooltip,
        wmDropTargetSetProps set_properties,
        short context, bool free, bool free_idname, bool free_tooltip)
{
	wmDropTarget *drop_target = MEM_callocN(sizeof(wmDropTarget), __func__);
	drop_target->size = size;
	drop_target->ot_idname = ot_idname;
	drop_target->tooltip = tooltip;
	drop_target->set_properties = set_properties;
	drop_target->context = context;
	drop_target->free = free;
	drop_target->free_idname = free_idname;
	drop_target->free_tooltip = free_tooltip;
	return drop_target;
}


/* ********************* Query Drag Data ********************* */

ID *WM_drag_query_single_id(wmDragData *drag_data)
{
	if (drag_data->type == DRAG_DATA_ID) {
		return drag_data->data.id;
	}
	else if (drag_data->type == DRAG_DATA_COLLECTION_CHILDREN) {
		ListBase *list = drag_data->data.collection_children;
		if (BLI_listbase_is_single(list)) {
			return (ID *)((wmDragCollectionChild *)((LinkData *)list->first)->data)->id;
		}
	}
	return NULL;
}

ID *WM_drag_query_single_id_of_type(wmDragData *drag_data, int idtype)
{
	ID *id = WM_drag_query_single_id(drag_data);
	if (id && GS(id->name) == idtype) return id;
	return NULL;
}

Collection *WM_drag_query_single_collection(wmDragData *drag_data)
{
	return (Collection *)WM_drag_query_single_id_of_type(drag_data, ID_GR);
}

Material *WM_drag_query_single_material(wmDragData *drag_data)
{
	return (Material *)WM_drag_query_single_id_of_type(drag_data, ID_MA);
}

Object *WM_drag_query_single_object(wmDragData *drag_data)
{
	return (Object *)WM_drag_query_single_id_of_type(drag_data, ID_OB);
}

const char *WM_drag_query_single_path(wmDragData *drag_data)
{
	if (drag_data->type == DRAG_DATA_FILEPATHS) {
		if (drag_data->data.filepaths.amount == 1) {
			return drag_data->data.filepaths.paths[0];
		}
	}
	return NULL;
}

const char *WM_drag_query_single_path_of_types(wmDragData *drag_data, int types)
{
	const char *path = WM_drag_query_single_path(drag_data);
	if (!path) return NULL;

	if (ED_path_extension_type(path) & types) {
		return path;
	}

	return NULL;
}

const char *WM_drag_query_single_path_text(wmDragData *drag_data)
{
	return WM_drag_query_single_path_of_types(drag_data, FILE_TYPE_PYSCRIPT | FILE_TYPE_TEXT);
}

const char *WM_drag_query_single_path_maybe_text(wmDragData *drag_data)
{
	const char *path = WM_drag_query_single_path(drag_data);
	if (!path) return NULL;

	int type = ED_path_extension_type(path);
	if (type == 0 || ELEM(type, FILE_TYPE_PYSCRIPT, FILE_TYPE_TEXT)) {
		return path;
	}

	return NULL;
}

const char *WM_drag_query_single_path_image(wmDragData *drag_data)
{
	return WM_drag_query_single_path_of_types(drag_data, FILE_TYPE_IMAGE);
}

const char *WM_drag_query_single_path_movie(wmDragData *drag_data)
{
	return WM_drag_query_single_path_of_types(drag_data, FILE_TYPE_MOVIE);
}

const char *WM_drag_query_single_path_sound(wmDragData *drag_data)
{
	return WM_drag_query_single_path_of_types(drag_data, FILE_TYPE_SOUND);
}

const char *WM_drag_query_single_path_image_or_movie(wmDragData *drag_data)
{
	return WM_drag_query_single_path_of_types(drag_data, FILE_TYPE_IMAGE | FILE_TYPE_MOVIE);
}

ListBase *WM_drag_query_collection_children(wmDragData *drag_data)
{
	if (drag_data->type == DRAG_DATA_COLLECTION_CHILDREN) {
		return drag_data->data.collection_children;
	}
	return NULL;
}

/* ********************* Draw ********************* */

void WM_drag_draw(bContext *UNUSED(C), wmWindow *win, wmDragOperation *drag_operation)
{
	wmDragData *drag_data = drag_operation->data;
	wmDropTarget *drop_target = drag_operation->target;

	int cursorx = win->eventstate->x;
	int cursory = win->eventstate->y;

	const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
	const uchar text_col[] = {255, 255, 255, 255};

	if (drop_target && drop_target->tooltip) {
		UI_fontstyle_draw_simple(fstyle, cursorx, cursory, drop_target->tooltip, text_col);
	}

	glEnable(GL_BLEND);

	if (drag_data->display_type == DRAG_DISPLAY_ICON) {
		UI_icon_draw(cursorx, cursory, drag_data->display.icon_id);
	}
	else if (drag_data->display_type == DRAG_DISPLAY_COLOR) {
		float color[4];
		copy_v3_v3(color, drag_data->display.color);
		color[3] = 1.0f;
		UI_draw_roundbox_4fv(true, cursorx - 5, cursory - 5, cursorx + 5, cursory + 5, 2, color);
	}

	glDisable(GL_BLEND);
}


/* ****************** Find Current Target ****************** */

static void drop_files_init(wmDragData *drag_data, PointerRNA *ptr)
{
	for (int i = 0; i < drag_data->data.filepaths.amount; i++) {
		char *path = drag_data->data.filepaths.paths[i];
		PointerRNA itemptr;
		RNA_collection_add(ptr, "filepaths", &itemptr);
		RNA_string_set(&itemptr, "name", path);
	}
}


static void get_window_drop_target(bContext *C, wmDropTargetFinder *finder, wmDragData *drag_data, const wmEvent *event)
{
	UI_drop_target_find(C, finder, drag_data, event);

	if (drag_data->type == DRAG_DATA_FILEPATHS) {
		WM_drop_target_propose__template_1(finder, DROP_TARGET_SIZE_WINDOW, "WM_OT_drop_files", "", drop_files_init);
	}
}

wmDropTarget *WM_drag_find_current_target(bContext *C, wmDragData *drag_data, const wmEvent *event)
{
	ScrArea *sa = CTX_wm_area(C);
	if (!sa) return NULL;

	wmDropTargetFinder finder = { 0 };

	SpaceType *st = sa->type;
	wmDropTarget *drop_target = NULL;

	if (!drop_target && st->drop_target_find) {
		st->drop_target_find(C, &finder, drag_data, event);
	}

	if (!drop_target) {
		get_window_drop_target(C, &finder, drag_data, event);
	}

	return finder.current;
}


/* ****************** Misc ****************** */

void WM_drag_transfer_ownership_to_event(struct wmWindowManager *wm, struct wmEvent * event)
{
	if (wm->drag.target) {
		WM_drop_target_free(wm->drag.target);
	}

	event->custom = EVT_DATA_DRAGDROP;
	event->customdata = wm->drag.data;
	event->customdatafree = true;

	wm->drag.data = NULL;
	wm->drag.target = NULL;
}

wmDragData *WM_drag_get_active(bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	return wm->drag.data;
}

wmDragData *WM_drag_data_from_event(const wmEvent *event)
{
	if (event->custom != EVT_DATA_DRAGDROP) return NULL;
	return (wmDragData *)event->customdata;
}

void WM_drop_init_single_filepath(wmDragData *drag_data, PointerRNA *ptr)
{
	RNA_string_set(ptr, "filepath", WM_drag_query_single_path(drag_data));
}

void WM_drop_init_single_id_name(wmDragData *drag_data, PointerRNA *ptr)
{
	RNA_string_set(ptr, "name", WM_drag_query_single_id(drag_data)->name + 2);
}