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

#include "BLI_profile_parse.hh"
#include "BLI_utildefines.h"

#include "DNA_userdef_types.h"

#include "BKE_context.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "info_intern.h"

using blender::profile::ProfileSegment;

namespace blender::ed::info {

static void draw_centered_label(
    uiBlock *block, StringRefNull str, int x, int y, int width, int height)
{
  uiBut *but = uiDefIconTextBut(block,
                                UI_BTYPE_LABEL,
                                0,
                                ICON_NONE,
                                str.c_str(),
                                x,
                                y,
                                width,
                                height,
                                nullptr,
                                0,
                                0,
                                0,
                                0,
                                nullptr);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_RIGHT);
}

static void info_profile_draw_impl(const bContext *C, ARegion *region)
{
  SpaceInfo *sinfo = CTX_wm_space_info(C);
  UNUSED_VARS(sinfo, region);

  UI_ThemeClearColor(TH_BACK);
  Vector<ProfileSegment> segments = profile::get_recorded_segments();

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS_NONE);

  for (const int i : segments.index_range()) {
    const ProfileSegment &segment = segments[i];
    const std::string str = std::to_string(segments.size());
    const int y = 50 + i * UI_UNIT_Y;
    draw_centered_label(block, segment.name, 50, y, 300, 100);
    const auto duration = segment.end_time - segment.begin_time;
    const std::string duration_str = std::to_string(duration.count() / 1000.0f) + " us";
    draw_centered_label(block, duration_str, 400, y, 300, 100);
  }
  UI_block_end(C, block);
  UI_block_draw(C, block);
}

}  // namespace blender::ed::info

void info_profile_draw(const bContext *C, ARegion *region)
{
  blender::ed::info::info_profile_draw_impl(C, region);
}
