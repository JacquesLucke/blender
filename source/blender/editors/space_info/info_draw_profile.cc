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

#include "BKE_context.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "info_intern.h"

using blender::profile::ProfileSegment;

namespace blender::ed::info {

static void info_profile_draw_impl(const bContext *C, ARegion *region)
{
  SpaceInfo *sinfo = CTX_wm_space_info(C);
  UNUSED_VARS(sinfo, region);

  UI_ThemeClearColor(TH_BACK);
  Vector<ProfileSegment> segments = profile::get_recorded_segments();

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS_NONE);
  const std::string str = std::to_string(segments.size());
  uiDefIconTextBut(block,
                   UI_BTYPE_LABEL,
                   0,
                   ICON_NONE,
                   str.c_str(),
                   50,
                   50,
                   100,
                   100,
                   nullptr,
                   0,
                   0,
                   0,
                   0,
                   nullptr);
  UI_block_end(C, block);
  UI_block_draw(C, block);
}

}  // namespace blender::ed::info

void info_profile_draw(const bContext *C, ARegion *region)
{
  blender::ed::info::info_profile_draw_impl(C, region);
}
