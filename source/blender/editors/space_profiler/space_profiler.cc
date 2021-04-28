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
 */

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "profiler_draw.hh"
#include "profiler_runtime.hh"

static SpaceLink *profiler_create(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
  SpaceProfiler *sprofiler = (SpaceProfiler *)MEM_callocN(sizeof(SpaceProfiler), "profiler space");
  sprofiler->spacetype = SPACE_PROFILER;

  {
    /* header */
    ARegion *region = (ARegion *)MEM_callocN(sizeof(ARegion), "profiler header");
    BLI_addtail(&sprofiler->regionbase, region);
    region->regiontype = RGN_TYPE_HEADER;
    region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;
  }

  {
    /* main window */
    ARegion *region = (ARegion *)MEM_callocN(sizeof(ARegion), "profiler main region");
    BLI_addtail(&sprofiler->regionbase, region);
    region->regiontype = RGN_TYPE_WINDOW;

    View2D *v2d = &region->v2d;
    v2d->scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM;
    v2d->keepzoom = V2D_LOCKZOOM_Y;
    v2d->keeptot = V2D_KEEPTOT_BOUNDS;
    v2d->keepofs = V2D_KEEPOFS_Y;
    v2d->align = V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y;
    v2d->min[0] = 0.0001f;
    v2d->min[1] = 1.0f;
    v2d->max[0] = 100000.0f;
    v2d->max[1] = 5000.0f;
    BLI_rctf_init(&v2d->tot, 0, 5000, -1000, 0);
    v2d->cur = v2d->tot;
  }

  return (SpaceLink *)sprofiler;
}

static void profiler_free(SpaceLink *sl)
{
  SpaceProfiler *sprofiler = (SpaceProfiler *)sl;
  delete sprofiler->runtime;
}

static void profiler_init(wmWindowManager *UNUSED(wm), ScrArea *area)
{
  SpaceProfiler *sprofiler = (SpaceProfiler *)area->spacedata.first;
  if (sprofiler->runtime == nullptr) {
    sprofiler->runtime = new SpaceProfiler_Runtime();
  }
}

static SpaceLink *profiler_duplicate(SpaceLink *sl)
{
  SpaceProfiler *sprofiler_old = (SpaceProfiler *)sl;
  SpaceProfiler *sprofiler_new = (SpaceProfiler *)MEM_dupallocN(sl);
  sprofiler_new->runtime = new SpaceProfiler_Runtime(*sprofiler_old->runtime);
  return (SpaceLink *)sprofiler_new;
}

static void profiler_keymap(wmKeyConfig *UNUSED(keyconf))
{
}

static void profiler_main_region_init(wmWindowManager *UNUSED(wm), ARegion *region)
{
  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_CUSTOM, region->winx, region->winy);
}

static void profiler_main_region_draw(const bContext *C, ARegion *region)
{
  blender::ed::profiler::draw_profiler(C, region);
}

static void profiler_main_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  wmNotifier *wmn = params->notifier;

  switch (wmn->category) {
    case NC_SPACE: {
      if (wmn->data == ND_SPACE_PROFILER) {
        ED_region_tag_redraw(region);
      }
      break;
    }
    case NC_PROFILE: {
      ED_region_tag_redraw(region);
      break;
    }
  }
}

static void profiler_header_region_init(wmWindowManager *UNUSED(wm), ARegion *region)
{
  ED_region_header_init(region);
}

static void profiler_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

static void profiler_header_region_free(ARegion *UNUSED(region))
{
}

static void profiler_header_region_listener(const wmRegionListenerParams *UNUSED(params))
{
}

static void profiler_operatortypes()
{
}

void ED_spacetype_profiler(void)
{
  SpaceType *st = (SpaceType *)MEM_callocN(sizeof(SpaceType), "spacetype profiler");
  ARegionType *art;

  st->spaceid = SPACE_PROFILER;
  strncpy(st->name, "Profiler", BKE_ST_MAXNAME);

  st->create = profiler_create;
  st->free = profiler_free;
  st->init = profiler_init;
  st->duplicate = profiler_duplicate;
  st->operatortypes = profiler_operatortypes;
  st->keymap = profiler_keymap;

  /* regions: main window */
  art = (ARegionType *)MEM_callocN(sizeof(ARegionType), "spacetype profiler region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;

  art->init = profiler_main_region_init;
  art->draw = profiler_main_region_draw;
  art->listener = profiler_main_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = (ARegionType *)MEM_callocN(sizeof(ARegionType), "spacetype profiler header region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = 0;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_HEADER;

  art->init = profiler_header_region_init;
  art->draw = profiler_header_region_draw;
  art->free = profiler_header_region_free;
  art->listener = profiler_header_region_listener;
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);
}
