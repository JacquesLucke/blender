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

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_immediate.h"

#include "info_intern.h"

using blender::profile::ProfileNode;
using blender::profile::ProfileResult;
using blender::profile::ProfileSegment;
using blender::profile::TimePoint;

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

static float factor_in_time_span(const TimePoint begin, const TimePoint end, const TimePoint time)
{
  if (begin == end) {
    return 0.0f;
  }
  const auto factor = (float)(time - begin).count() / (float)(end - begin).count();
  return factor;
}

static void info_profile_draw_impl(const bContext *C, ARegion *region)
{
  SpaceInfo *sinfo = CTX_wm_space_info(C);
  UNUSED_VARS(sinfo, region);

  UI_ThemeClearColor(TH_BACK);
  Vector<ProfileSegment> segments = profile::get_recorded_segments();

  ProfileResult profile_result;
  profile_result.add(segments);

  const TimePoint begin_time = profile_result.begin_time();
  const TimePoint end_time = profile_result.end_time();

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor4f(0.8f, 0.8f, 0.3f, 1.0f);

  const int left_x = 0;
  const int right_x = region->winx;
  const int top_y = 100;
  const int bottom_y = 50;

  for (const ProfileNode *node : profile_result.root_nodes()) {
    const float begin_factor = factor_in_time_span(begin_time, end_time, node->begin_time());
    const float end_factor = factor_in_time_span(begin_time, end_time, node->end_time());

    const int begin_x = left_x + (right_x - left_x) * begin_factor;
    const int end_x = std::max<int>(left_x + (right_x - left_x) * end_factor, begin_x + 1);

    immRecti(pos, begin_x, top_y, end_x, bottom_y);
  }

  immUnbindProgram();

  // uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS_NONE);
  // UI_block_end(C, block);
  // UI_block_draw(C, block);
}

}  // namespace blender::ed::info

void info_profile_draw(const bContext *C, ARegion *region)
{
  blender::ed::info::info_profile_draw_impl(C, region);
}
