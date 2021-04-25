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
#include "BLI_math_color.h"
#include "BLI_profile.hh"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_immediate.h"

#include "ED_info.h"

#include "info_intern.hh"
#include "info_profile_layout.hh"
#include "info_runtime.hh"

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
                                std::min(width, INT16_MAX),
                                std::min(height, INT16_MAX),
                                nullptr,
                                0,
                                0,
                                0,
                                0,
                                nullptr);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_RIGHT);
}

static void set_color_based_on_time(const TimePoint time)
{
  const uint64_t value = time.time_since_epoch().count();
  const float variation = BLI_hash_int_2d_to_float(value, value >> 32);
  float r, g, b;
  hsv_to_rgb(variation * 0.2f, 0.5f, 0.5f, &r, &g, &b);
  immUniformColor4f(r, g, b, 1.0f);
}

#define ROW_HEIGHT UI_UNIT_Y
#define THREAD_PADDING ((int)(ROW_HEIGHT * 0.2f))
#define ROOT_PADDING UI_UNIT_Y

static void draw_profile_nodes(uiBlock *block,
                               Span<const ProfileNode *> nodes,
                               FunctionRef<int(TimePoint)> time_to_x,
                               int &top_y);

static void draw_profile_node_recursively(uiBlock *block,
                                          const ProfileNode &node,
                                          FunctionRef<int(TimePoint)> time_to_x,
                                          int &top_y)
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  set_color_based_on_time(node.begin_time());

  const int left_x = time_to_x(node.begin_time());
  const int right_x = std::max<int>(time_to_x(node.end_time()), left_x + 1);
  const int bottom_y = top_y - UI_UNIT_Y;
  immRecti(pos, left_x, top_y, right_x, bottom_y);

  immUnbindProgram();

  draw_centered_label(
      block, node.name().c_str(), left_x, top_y - UI_UNIT_Y, right_x - left_x, UI_UNIT_Y);

  top_y -= UI_UNIT_Y;
  draw_profile_nodes(block, node.children_on_same_thread(), time_to_x, top_y);

  for (Span<const ProfileNode *> nodes : node.stacked_children_in_other_threads()) {
    top_y -= THREAD_PADDING;
    draw_profile_nodes(block, nodes, time_to_x, top_y);
  }
}

static void draw_profile_nodes(uiBlock *block,
                               Span<const ProfileNode *> nodes,
                               FunctionRef<int(TimePoint)> time_to_x,
                               int &top_y)
{
  int new_top_y = top_y;
  for (const ProfileNode *node : nodes) {
    int sub_top_y = top_y;
    draw_profile_node_recursively(block, *node, time_to_x, sub_top_y);
    new_top_y = std::min(new_top_y, sub_top_y);
  }
  top_y = new_top_y;
}

void info_profile_draw(const bContext *C, ARegion *region)
{
  BLI_SCOPED_PROFILE(__func__);

  SpaceInfo *sinfo = CTX_wm_space_info(C);
  SpaceInfo_Runtime &runtime = *sinfo->runtime;
  if (!runtime.profile_layout) {
    runtime.profile_layout = std::make_unique<ProfileLayout>();
  }
  ProfileLayout &profile_layout = *runtime.profile_layout;

  UI_ThemeClearColor(TH_BACK);
  RecordedProfile recorded_profile = profile::extract_recorded_profile();
  profile_layout.add(recorded_profile);

  const TimePoint begin_time = profile_layout.begin_time();
  const TimePoint end_time = profile_layout.end_time();

  const auto time_to_x = [&](TimePoint time) -> int {
    if (time == TimePoint{}) {
      time = end_time;
    }
    const Duration time_since_begin = time - begin_time;
    const float x = UI_view2d_view_to_region_x(&region->v2d,
                                               time_since_begin.count() / 1000000.0f);
    return x;
  };

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS_NONE);

  int top_y = region->winy;
  const Span<uint64_t> root_thread_ids = profile_layout.root_thread_ids();
  for (const uint64_t root_thread_id : root_thread_ids) {
    const Span<const ProfileNode *> root_nodes = profile_layout.root_nodes_by_thread_id(
        root_thread_id);
    int next_top_y = top_y;
    draw_profile_nodes(block, root_nodes, time_to_x, next_top_y);
    top_y = next_top_y - ROOT_PADDING;
  }

  UI_block_end(C, block);
  UI_block_draw(C, block);

  const float duration_ms = (end_time - begin_time).count() / 1000000.0f;
  UI_view2d_totRect_set(&region->v2d, duration_ms, 100.0f);

  UI_view2d_scrollers_draw(&region->v2d, nullptr);
}

}  // namespace blender::ed::info

void ED_info_profile_enable(SpaceInfo *sinfo)
{
  sinfo->runtime->is_recording_profile = true;
}

void ED_info_profile_disable(SpaceInfo *sinfo)
{
  sinfo->runtime->is_recording_profile = false;
}

bool ED_info_profile_is_enabled(SpaceInfo *sinfo)
{
  return sinfo->runtime->is_recording_profile;
}
