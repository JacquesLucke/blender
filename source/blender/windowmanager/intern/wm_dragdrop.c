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

#include "GPU_shader.h"

#include "IMB_imbuf_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"

/* ****************************************************** */

static ListBase dropboxes = {NULL, NULL};

/* drop box maps are stored global for now */
/* these are part of blender's UI/space specs, and not like keymaps */
/* when editors become configurable, they can add own dropbox definitions */

typedef struct wmDropBoxMap {
	struct wmDropBoxMap *next, *prev;

	ListBase dropboxes;
	short spaceid, regionid;
	char idname[KMAP_MAX_NAME];

} wmDropBoxMap;

/* spaceid/regionid is zero for window drop maps */
ListBase *WM_dropboxmap_find(const char *idname, int spaceid, int regionid)
{
	wmDropBoxMap *dm;

	for (dm = dropboxes.first; dm; dm = dm->next)
		if (dm->spaceid == spaceid && dm->regionid == regionid)
			if (STREQLEN(idname, dm->idname, KMAP_MAX_NAME))
				return &dm->dropboxes;

	dm = MEM_callocN(sizeof(struct wmDropBoxMap), "dropmap list");
	BLI_strncpy(dm->idname, idname, KMAP_MAX_NAME);
	dm->spaceid = spaceid;
	dm->regionid = regionid;
	BLI_addtail(&dropboxes, dm);

	return &dm->dropboxes;
}



wmDropBox *WM_dropbox_add(
        ListBase *lb, const char *idname,
        bool (*poll)(bContext *, wmDrag *, const wmEvent *, const char **),
        void (*copy)(wmDrag *, wmDropBox *))
{
	return NULL;
	wmDropBox *drop = MEM_callocN(sizeof(wmDropBox), "wmDropBox");

	drop->poll = poll;
	drop->copy = copy;
	drop->ot = WM_operatortype_find(idname, 0);
	drop->opcontext = WM_OP_INVOKE_DEFAULT;

	if (drop->ot == NULL) {
		MEM_freeN(drop);
		printf("Error: dropbox with unknown operator: %s\n", idname);
		return NULL;
	}
	WM_operator_properties_alloc(&(drop->ptr), &(drop->properties), idname);

	BLI_addtail(lb, drop);

	return drop;
}

void wm_dropbox_free(void)
{
	wmDropBoxMap *dm;

	for (dm = dropboxes.first; dm; dm = dm->next) {
		wmDropBox *drop;

		for (drop = dm->dropboxes.first; drop; drop = drop->next) {
			if (drop->ptr) {
				WM_operator_properties_free(drop->ptr);
				MEM_freeN(drop->ptr);
			}
		}
		BLI_freelistN(&dm->dropboxes);
	}

	BLI_freelistN(&dropboxes);
}

/* *********************************** */

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
	if (drop_target->free) {
		if (drop_target->free_tooltip) {
			MEM_freeN(drop_target->tooltip);
		}
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

static DropTarget *new_drop_target(void)
{
	DropTarget *drop_target = MEM_callocN(sizeof(DropTarget), __func__);
	return drop_target;
}

DropTarget *WM_event_get_active_droptarget(bContext *C, DragData *drag_data, const wmEvent *event)
{
	if (event->shift && CTX_wm_space_outliner(C) || drag_data->type == DRAG_DATA_FILEPATHS) {
		DropTarget *drop_target = new_drop_target();
		drop_target->ot_idname = "WM_OT_window_new";
		drop_target->tooltip = "Make new window";
		return drop_target;
	}
	return NULL;
}

void WM_event_update_current_droptarget(bContext *C, DragOperationData *drag_operation, const wmEvent *event)
{
	if (drag_operation->current_target) {
		WM_drop_target_free(drag_operation->current_target);
	}
	drag_operation->current_target = WM_event_get_active_droptarget(C, drag_operation->drag_data, event);
}

static const char *dropbox_active(bContext *C, ListBase *handlers, wmDrag *drag, const wmEvent *event)
{
	wmEventHandler *handler = handlers->first;
	for (; handler; handler = handler->next) {
		if (handler->dropboxes) {
			wmDropBox *drop = handler->dropboxes->first;
			for (; drop; drop = drop->next) {
				const char *tooltip = NULL;
				if (drop->poll(C, drag, event, &tooltip)) {
					/* XXX Doing translation here might not be ideal, but later we have no more
					 *     access to ot (and hence op context)... */
					return (tooltip) ? tooltip : RNA_struct_ui_name(drop->ot->srna);
				}
			}
		}
	}
	return NULL;
}

/* return active operator name when mouse is in box */
static const char *wm_dropbox_active(bContext *C, wmDrag *drag, const wmEvent *event)
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	const char *name;

	name = dropbox_active(C, &win->handlers, drag, event);
	if (name) return name;

	name = dropbox_active(C, &sa->handlers, drag, event);
	if (name) return name;

	name = dropbox_active(C, &ar->handlers, drag, event);
	if (name) return name;

	return NULL;
}

/* ************** IDs ***************** */

void WM_drag_add_ID(wmDrag *drag, ID *id, ID *from_parent)
{
	/* Don't drag the same ID twice. */
	for (wmDragID *drag_id = drag->ids.first; drag_id; drag_id = drag_id->next) {
		if (drag_id->id == id) {
			if (drag_id->from_parent == NULL) {
				drag_id->from_parent = from_parent;
			}
			return;
		}
		else if (GS(drag_id->id->name) != GS(id->name)) {
			BLI_assert(!"All dragged IDs must have the same type");
			return;
		}
	}

	/* Add to list. */
	wmDragID *drag_id = MEM_callocN(sizeof(wmDragID), __func__);
	drag_id->id = id;
	drag_id->from_parent = from_parent;
	BLI_addtail(&drag->ids, drag_id);
}

ID *WM_drag_ID(const wmDrag *drag, short idcode)
{
	if (drag->type != WM_DRAG_ID) {
		return NULL;
	}

	wmDragID *drag_id = drag->ids.first;
	if (!drag_id) {
		return NULL;
	}

	ID *id = drag_id->id;
	return (idcode == 0 || GS(id->name) == idcode) ? id : NULL;

}

ID *WM_drag_ID_from_event(const wmEvent *event, short idcode)
{
	if (event->custom != EVT_DATA_DRAGDROP) {
		return NULL;
	}

	ListBase *lb = event->customdata;
	return WM_drag_ID(lb->first, idcode);
}

/* ************** draw ***************** */

static void wm_drop_operator_draw(const char *name, int x, int y)
{
	const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
	const float col_fg[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	const float col_bg[4] = {0.0f, 0.0f, 0.0f, 0.2f};

	UI_fontstyle_draw_simple_backdrop(fstyle, x, y, name, col_fg, col_bg);
}

static const char *wm_drag_name(wmDrag *drag)
{
	switch (drag->type) {
		case WM_DRAG_ID:
		{
			ID *id = WM_drag_ID(drag, 0);
			bool single = (BLI_listbase_count_at_most(&drag->ids, 2) == 1);

			if (single) {
				return id->name + 2;
			}
			else if (id) {
				return BKE_idcode_to_name_plural(GS(id->name));
			}
			break;
		}
		case WM_DRAG_PATH:
		case WM_DRAG_NAME:
			return drag->path;
	}
	return "";
}

static void drag_rect_minmax(rcti *rect, int x1, int y1, int x2, int y2)
{
	if (rect->xmin > x1)
		rect->xmin = x1;
	if (rect->xmax < x2)
		rect->xmax = x2;
	if (rect->ymin > y1)
		rect->ymin = y1;
	if (rect->ymax < y2)
		rect->ymax = y2;
}

void wm_draw_drag_data(bContext *C, wmWindow *win, DragOperationData *drag_operation)
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

