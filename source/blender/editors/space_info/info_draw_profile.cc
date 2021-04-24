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

#include "BLI_function_ref.hh"
#include "BLI_hash.h"
#include "BLI_profile.hh"
#include "BLI_utildefines.h"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_immediate.h"

#include "info_intern.h"
#include "info_profile_layout.hh"

using blender::profile::Duration;
using blender::profile::RecordedProfile;
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

static void draw_profile_node_recursively(uiBlock *block,
                                          const ProfileNode &node,
                                          FunctionRef<int(TimePoint)> time_to_x,
                                          int &current_top_y)
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor4f(0.5f, 0.4f, 0.1f, 1.0f);

  const int left_x = time_to_x(node.begin_time());
  const int right_x = std::max<int>(time_to_x(node.end_time()), left_x + 1);
  const int bottom_y = current_top_y - UI_UNIT_Y;
  immRecti(pos, left_x, current_top_y, right_x, bottom_y);

  immUnbindProgram();

  draw_centered_label(
      block, node.name().c_str(), left_x, current_top_y - UI_UNIT_Y, right_x - left_x, UI_UNIT_Y);

  current_top_y -= UI_UNIT_Y;
  for (const ProfileNode *child : node.children_on_same_thread()) {
    int sub_top_y = current_top_y;
    draw_profile_node_recursively(block, *child, time_to_x, sub_top_y);
  }
}

static void info_profile_draw_impl(const bContext *C, ARegion *region)
{
  SpaceInfo *sinfo = CTX_wm_space_info(C);
  UNUSED_VARS(sinfo, region);

  UI_ThemeClearColor(TH_BACK);
  RecordedProfile recorded_profile = profile::get_recorded_profile();
  ProfileLayout profile_layout;
  profile_layout.add(recorded_profile);

  const TimePoint end_time = profile_layout.end_time();
  const Duration time_to_display = std::chrono::seconds(5);

  const auto time_to_x = [&](TimePoint time) -> int {
    if (time == TimePoint{}) {
      time = end_time;
    }
    const Duration duration_to_end = end_time - time;
    const float factor = (float)duration_to_end.count() / (float)time_to_display.count();
    return (1 - factor) * region->winx;
  };

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS_NONE);

  const Span<uint64_t> root_thread_ids = profile_layout.root_thread_ids();
  for (const uint64_t root_thread_id : root_thread_ids) {
    const Span<const ProfileNode *> root_nodes = profile_layout.root_nodes_by_thread_id(
        root_thread_id);

    for (const ProfileNode *node : root_nodes) {
      int current_top_y = region->winy;
      draw_profile_node_recursively(block, *node, time_to_x, current_top_y);
    }
  }

  UI_block_end(C, block);
  UI_block_draw(C, block);
}

}  // namespace blender::ed::info

void info_profile_draw(const bContext *C, ARegion *region)
{
  blender::ed::info::info_profile_draw_impl(C, region);
}
