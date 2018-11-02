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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_console/space_console.c
 *  \ingroup spconsole
 */

#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "console_intern.h" // own include
#include "GPU_framebuffer.h"

/* ******************** default callbacks for console space ***************** */

static SpaceLink *console_new(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
	ARegion *ar;
	SpaceConsole *sconsole;

	sconsole = MEM_callocN(sizeof(SpaceConsole), "initconsole");
	sconsole->spacetype = SPACE_CONSOLE;

	sconsole->lheight =  14;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for console");

	BLI_addtail(&sconsole->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_TOP;


	/* main region */
	ar = MEM_callocN(sizeof(ARegion), "main region for text");

	BLI_addtail(&sconsole->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;

	/* keep in sync with info */
	ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
	ar->v2d.align |= V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_NEG_Y; /* align bottom left */
	ar->v2d.keepofs |= V2D_LOCKOFS_X;
	ar->v2d.keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
	ar->v2d.keeptot = V2D_KEEPTOT_BOUNDS;
	ar->v2d.minzoom = ar->v2d.maxzoom = 1.0f;

	/* for now, aspect ratio should be maintained, and zoom is clamped within sane default limits */
	//ar->v2d.keepzoom = (V2D_KEEPASPECT|V2D_LIMITZOOM);

	return (SpaceLink *)sconsole;
}

/* not spacelink itself */
static void console_free(SpaceLink *sl)
{
	SpaceConsole *sc = (SpaceConsole *) sl;

	while (sc->scrollback.first)
		console_scrollback_free(sc, sc->scrollback.first);

	while (sc->history.first)
		console_history_free(sc, sc->history.first);
}


/* spacetype; init callback */
static void console_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{

}

static SpaceLink *console_duplicate(SpaceLink *sl)
{
	SpaceConsole *sconsolen = MEM_dupallocN(sl);

	/* clear or remove stuff from old */

	/* TODO - duplicate?, then we also need to duplicate the py namespace */
	BLI_listbase_clear(&sconsolen->scrollback);
	BLI_listbase_clear(&sconsolen->history);

	return (SpaceLink *)sconsolen;
}



/* add handlers, stuff you only do once or on area/region changes */
static void console_main_region_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	const float prev_y_min = ar->v2d.cur.ymin; /* so re-sizing keeps the cursor visible */

	/* force it on init, for old files, until it becomes config */
	ar->v2d.scroll = (V2D_SCROLL_RIGHT);

	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);

	/* always keep the bottom part of the view aligned, less annoying */
	if (prev_y_min != ar->v2d.cur.ymin) {
		const float cur_y_range = BLI_rctf_size_y(&ar->v2d.cur);
		ar->v2d.cur.ymin = prev_y_min;
		ar->v2d.cur.ymax = prev_y_min + cur_y_range;
	}

	/* own keymap */
	keymap = WM_keymap_ensure(wm->defaultconf, "Console", SPACE_CONSOLE, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

/* same as 'text_cursor' */
static void console_cursor(wmWindow *win, ScrArea *sa, ARegion *ar)
{
	SpaceText *st = sa->spacedata.first;
	int wmcursor = BC_TEXTEDITCURSOR;

	if (st->text && BLI_rcti_isect_pt(&st->txtbar, win->eventstate->x - ar->winrct.xmin, st->txtbar.ymin)) {
		wmcursor = CURSOR_STD;
	}

	WM_cursor_set(win, wmcursor);
}

/* ************* end drop *********** */

static void console_main_region_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceConsole *sc = CTX_wm_space_console(C);
	View2D *v2d = &ar->v2d;
	View2DScrollers *scrollers;

	if (BLI_listbase_is_empty(&sc->scrollback))
		WM_operator_name_call((bContext *)C, "CONSOLE_OT_banner", WM_OP_EXEC_DEFAULT, NULL);

	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	GPU_clear(GPU_COLOR_BIT);

	/* worlks best with no view2d matrix set */
	UI_view2d_view_ortho(v2d);

	/* data... */

	console_history_verify(C); /* make sure we have some command line */
	console_textview_main(sc, ar);

	/* reset view matrix */
	UI_view2d_view_restore(C);

	/* scrollers */
	scrollers = UI_view2d_scrollers_calc(C, v2d, NULL, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_GRID_CLAMP);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

static void console_operatortypes(void)
{
	/* console_ops.c */
	WM_operatortype_append(CONSOLE_OT_move);
	WM_operatortype_append(CONSOLE_OT_delete);
	WM_operatortype_append(CONSOLE_OT_insert);

	WM_operatortype_append(CONSOLE_OT_indent);
	WM_operatortype_append(CONSOLE_OT_unindent);

	/* for use by python only */
	WM_operatortype_append(CONSOLE_OT_history_append);
	WM_operatortype_append(CONSOLE_OT_scrollback_append);

	WM_operatortype_append(CONSOLE_OT_clear);
	WM_operatortype_append(CONSOLE_OT_clear_line);
	WM_operatortype_append(CONSOLE_OT_history_cycle);
	WM_operatortype_append(CONSOLE_OT_copy);
	WM_operatortype_append(CONSOLE_OT_paste);
	WM_operatortype_append(CONSOLE_OT_select_set);
	WM_operatortype_append(CONSOLE_OT_select_word);
}

static void console_keymap(struct wmKeyConfig *keyconf)
{
	WM_keymap_ensure(keyconf, "Console", SPACE_CONSOLE, 0);
}

/****************** header region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void console_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void console_header_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void console_main_region_listener(
        wmWindow *UNUSED(win), ScrArea *sa, ARegion *ar,
        wmNotifier *wmn, const Scene *UNUSED(scene))
{
	// SpaceInfo *sinfo = sa->spacedata.first;

	/* context changes */
	switch (wmn->category) {
		case NC_SPACE:
		{
			if (wmn->data == ND_SPACE_CONSOLE) {
				if (wmn->action == NA_EDITED) {
					if ((wmn->reference && sa) && (wmn->reference == sa->spacedata.first)) {
						/* we've modified the geometry (font size), re-calculate rect */
						console_textview_update_rect(wmn->reference, ar);
						ED_region_tag_redraw(ar);
					}
				}
				else {
					/* generic redraw request */
					ED_region_tag_redraw(ar);
				}
			}
			break;
		}
	}
}

static void drop_init__insert_id_path(wmDragData *drag_data, PointerRNA *ptr)
{
	ID *id = WM_drag_query_single_id(drag_data);
	char *text = RNA_path_full_ID_py(id);
	RNA_string_set(ptr, "text", text);
	MEM_freeN(text);
}

static wmDropTarget *console_drop_target_get(bContext *UNUSED(C), wmDragData *drag_data, const wmEvent *UNUSED(event))
{
	if (WM_drag_query_single_id(drag_data)) {
		return WM_drop_target_new("CONSOLE_OT_insert", "Insert", drop_init__insert_id_path);
	}
	return NULL;
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_console(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype console");
	ARegionType *art;

	st->spaceid = SPACE_CONSOLE;
	strncpy(st->name, "Console", BKE_ST_MAXNAME);

	st->new = console_new;
	st->free = console_free;
	st->init = console_init;
	st->duplicate = console_duplicate;
	st->operatortypes = console_operatortypes;
	st->keymap = console_keymap;
	st->drop_target_get = console_drop_target_get;

	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype console region");
	art->regionid = RGN_TYPE_WINDOW;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;

	art->init = console_main_region_init;
	art->draw = console_main_region_draw;
	art->cursor = console_cursor;
	art->listener = console_main_region_listener;



	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype console region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

	art->init = console_header_region_init;
	art->draw = console_header_region_draw;

	BLI_addhead(&st->regiontypes, art);


	BKE_spacetype_register(st);
}
