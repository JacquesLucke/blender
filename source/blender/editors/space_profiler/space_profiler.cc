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
  }

  return (SpaceLink *)sprofiler;
}

static void profiler_free(SpaceLink *UNUSED(sl))
{
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

static void profiler_main_region_init(wmWindowManager *UNUSED(wm), ARegion *UNUSED(region))
{
}

static void profiler_main_region_draw(const bContext *C, ARegion *region)
{
  blender::ed::profiler::draw_profiler(C, region);
}

static void profiler_main_region_listener(const wmRegionListenerParams *UNUSED(params))
{
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
  art->keymapflag = ED_KEYMAP_UI;

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
