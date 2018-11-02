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

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BLI_blenlib.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_context.h"
#include "BKE_idcode.h"
#include "BKE_screen.h"

#include "GPU_shader.h"

#include "IMB_imbuf_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"

static DragData *WM_drag_data_new(void) {
	return MEM_callocN(sizeof(DragData), "drag data");
}

void WM_drag_data_free(DragData *drag_data)
{
	// TODO: free other data
	MEM_freeN(drag_data);
}

void WM_drop_target_free(DropTarget *drop_target)
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

void WM_drag_operation_free(DragOperationData *drag_operation)
{
	if (drag_operation->drag_data) {
		WM_drag_data_free(drag_operation->drag_data);
	}
	if (drag_operation->current_target) {
		WM_drop_target_free(drag_operation->current_target);
	}
}

static void start_dragging_data(struct bContext *C, DragData *drag_data)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wm->drag_operation = MEM_callocN(sizeof(DragOperationData), __func__);
	wm->drag_operation->drag_data = drag_data;
	wm->drag_operation->current_target = NULL;
}

DragData *WM_event_start_drag_id(struct bContext *C, ID *id)
{
	DragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_ID;
	drag_data->data.id = id;

	start_dragging_data(C, drag_data);
	return drag_data;
}

DragData *WM_event_start_drag_filepath(struct bContext *C, const char *filepath)
{
	char **paths = MEM_malloc_arrayN(1, sizeof(char *), __func__);
	paths[0] = BLI_strdup(filepath);

	DragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_FILEPATHS;
	drag_data->data.filepaths.amount = 1;
	drag_data->data.filepaths.paths = paths;

	start_dragging_data(C, drag_data);
	return drag_data;
}

DragData *WM_event_start_drag_color(struct bContext *C, float color[3], bool gamma_corrected)
{
	DragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_COLOR;
	memcpy(drag_data->data.color.color, color, sizeof(float) * 3);
	drag_data->data.color.gamma_corrected = gamma_corrected;

	start_dragging_data(C, drag_data);
	return drag_data;
}

DragData *WM_event_start_drag_value(struct bContext *C, double value)
{
	DragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_VALUE;
	drag_data->data.value = value;

	start_dragging_data(C, drag_data);
	return drag_data;
}

DragData *WM_event_start_drag_rna(struct bContext *C, struct PointerRNA *rna)
{
	DragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_RNA;
	drag_data->data.rna = rna;

	start_dragging_data(C, drag_data);
	return drag_data;
}

DragData *WM_event_start_drag_name(struct bContext *C, const char *name)
{
	DragData *drag_data = WM_drag_data_new();
	drag_data->type = DRAG_DATA_NAME;
	drag_data->data.name = BLI_strdup(name);

	start_dragging_data(C, drag_data);
	return drag_data;
}

void WM_event_drag_set_display_image(
        DragData *drag_data, ImBuf *imb,
        float scale, int width, int height)
{
	drag_data->display_type = DRAG_DISPLAY_IMAGE;
	drag_data->display.image.imb = imb;
	drag_data->display.image.scale = scale;
	drag_data->display.image.width = width;
	drag_data->display.image.height = height;
}

void WM_transfer_drag_data_ownership_to_event(struct wmWindowManager *wm, struct wmEvent * event)
{
	event->custom = EVT_DATA_DRAGDROP;
	event->customdata = wm->drag_operation;
	event->customdatafree = true;
	wm->drag_operation = NULL;
}

static DropTarget *new_empty_drop_target(void)
{
	return MEM_callocN(sizeof(DropTarget), __func__);
}

DropTarget *WM_drop_target_new(
        const char *ot_idname, const char *tooltip,
        void (*set_properties)(struct DragData *, struct PointerRNA *))
{
	return WM_drop_target_new_ex(
	        (char *)ot_idname, (char *)tooltip, set_properties,
	        WM_OP_INVOKE_DEFAULT, true, false, false);
}

DropTarget *WM_drop_target_new_ex(
        char *ot_idname, char *tooltip,
        void (*set_properties)(struct DragData *, struct PointerRNA *),
        short context, bool free, bool free_idname, bool free_tooltip)
{
	DropTarget *drop_target = MEM_callocN(sizeof(DropTarget), __func__);
	drop_target->ot_idname = ot_idname;
	drop_target->tooltip = tooltip;
	drop_target->set_properties = set_properties;
	drop_target->context = context;
	drop_target->free = free;
	drop_target->free_idname = free_idname;
	drop_target->free_tooltip = free_tooltip;
	return drop_target;
}

void set_props(DragData *drag_data, PointerRNA *ptr)
{
	RNA_property_string_set(ptr, "url", "www.blender.org");
}

DropTarget *get_window_drop_target(bContext *C, DragData *drag_data, const wmEvent *event)
{
	if (event->shift) {
		return WM_drop_target_new("WM_OT_url_open", "open url", set_props);
	}
	return NULL;
}

DropTarget *WM_event_get_active_droptarget(bContext *C, DragData *drag_data, const wmEvent *event)
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);
	SpaceType *st = sa->type;

	DropTarget *drop_target = NULL;

	if (!drop_target && st->drop_target_get) {
		drop_target = st->drop_target_get(C, drag_data, event);
	}

	if (!drop_target) {
		drop_target = get_window_drop_target(C, drag_data, event);
	}

	return drop_target;
}

void WM_event_update_current_droptarget(bContext *C, DragOperationData *drag_operation, const wmEvent *event)
{
	if (drag_operation->current_target) {
		WM_drop_target_free(drag_operation->current_target);
	}
	drag_operation->current_target = WM_event_get_active_droptarget(C, drag_operation->drag_data, event);
}

void wm_draw_drag_data(bContext *UNUSED(C), wmWindow *win, DragOperationData *drag_operation)
{
	DragData *drag_data = drag_operation->drag_data;
	DropTarget *drop_target = drag_operation->current_target;

	int cursorx = win->eventstate->x;
	int cursory = win->eventstate->y;

	const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
	const uchar text_col[] = {255, 255, 255, 255};

	if (drop_target && drop_target->tooltip) {
		UI_fontstyle_draw_simple(fstyle, cursorx, cursory, drop_target->tooltip, text_col);
	}
}

