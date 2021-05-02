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

#include <iomanip>

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_immediate.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"

#include "BLI_color.hh"
#include "BLI_hash.h"
#include "BLI_math_color.h"
#include "BLI_profile.hh"
#include "BLI_rect.h"

#include "profiler_draw.hh"
#include "profiler_layout.hh"
#include "profiler_runtime.hh"

namespace blender::ed::profiler {

using profile::Duration;

class ProfilerDrawer {
 private:
  const bContext *C;
  ARegion *region_;
  SpaceProfiler *sprofiler_;
  SpaceProfiler_Runtime *runtime_;
  ProfilerLayout *profiler_layout_;

 public:
  ProfilerDrawer(const bContext *C, ARegion *region) : C(C), region_(region)
  {
    sprofiler_ = CTX_wm_space_profiler(C);
    runtime_ = sprofiler_->runtime;

    if (!runtime_->profiler_layout) {
      runtime_->profiler_layout = std::make_unique<ProfilerLayout>();
    }
    if (runtime_->profile_listener) {
      profile::ProfileListener::flush_to_all();
    }
    profiler_layout_ = runtime_->profiler_layout.get();
  }

  void draw()
  {
    UI_ThemeClearColor(TH_BACK);

    Vector<ProfileNode *> nodes_to_draw;
    this->find_all_nodes_to_draw(nodes_to_draw);
    this->draw_nodes(nodes_to_draw);

    this->update_view2d_bounds();
  }

  void update_view2d_bounds()
  {
    const float duration_ms = duration_to_ms(profiler_layout_->end_time() -
                                             profiler_layout_->begin_time());

    float min_bottom_y = 0.0f;
    if (!profiler_layout_->root_nodes().is_empty()) {
      for (ProfileNode *node : profiler_layout_->root_nodes().last()) {
        min_bottom_y = std::min(min_bottom_y, node->bottom_y);
      }
    }

    /* Giving a bit more space on the right side is convenient. */
    const int width = std::max(duration_ms * 1.1f, 5000.0f);
    const int height = -min_bottom_y * UI_UNIT_Y + UI_UNIT_Y;

    UI_view2d_totRect_set(&region_->v2d, width, height);

    UI_view2d_scrollers_draw(&region_->v2d, nullptr);
  }

  void find_all_nodes_to_draw(Vector<ProfileNode *> &r_nodes)
  {
    BLI_PROFILE_SCOPE(__func__);
    for (Span<ProfileNode *> nodes : profiler_layout_->root_nodes()) {
      this->find_nodes_to_draw(nodes, r_nodes);
    }
  }

  void find_nodes_to_draw(Span<ProfileNode *> nodes, Vector<ProfileNode *> &r_nodes)
  {
    if (nodes.is_empty()) {
      return;
    }
    const float top_y = this->node_y_to_region_y(nodes[0]->top_y);
    if (top_y < 0) {
      return;
    }
    float node_bottom_y = nodes[0]->bottom_y;
    for (ProfileNode *node : nodes) {
      node_bottom_y = std::min(node_bottom_y, node->bottom_y);
    }
    const float bottom_y = this->node_y_to_region_y(node_bottom_y);
    if (bottom_y > region_->winy) {
      return;
    }

    const TimePoint left_time = this->x_to_time(0);
    const TimePoint right_time = this->x_to_time(region_->winx);

    auto end_is_smaller = [](const ProfileNode *node, const TimePoint time) {
      return node->end_time() < time;
    };
    auto begin_is_larger = [](const TimePoint time, const ProfileNode *node) {
      return node->begin_time() > time;
    };

    const int start_index = std::lower_bound(
                                nodes.begin(), nodes.end(), left_time, end_is_smaller) -
                            nodes.begin();
    const int end_index = std::upper_bound(
                              nodes.begin(), nodes.end(), right_time, begin_is_larger) -
                          nodes.begin();

    nodes = nodes.slice(start_index, end_index - start_index);
    if (nodes.is_empty()) {
      return;
    }

    const int size_before = r_nodes.size();
    r_nodes.append(nodes[0]);
    for (ProfileNode *node : nodes.drop_front(1)) {
      const float begin_x = this->time_to_x(node->begin_time());
      const float end_x = this->time_to_x(node->end_time());

      ProfileNode *prev_node = r_nodes.last();
      const float prev_begin_x = this->time_to_x(prev_node->begin_time());
      const float prev_end_x = this->time_to_x(prev_node->end_time());

      if (std::ceil(end_x) > std::ceil(prev_end_x)) {
        /* Node reaches into next pixel. */
        r_nodes.append(node);
      }
      else if (std::floor(begin_x) > std::floor(prev_begin_x)) {
        /* Previous node reaches into previous pixel. */
        r_nodes.append(node);
      }
      else {
        /* Both nodes are in the same pixel. */
        if (node->bottom_y < prev_node->bottom_y) {
          /* Replace previously added node because this when has a larger depth. */
          r_nodes.last() = node;
        }
      }
    }
    const int tot_added = r_nodes.size() - size_before;

    Vector<ProfileNode *> added_nodes = r_nodes.as_span().take_back(tot_added);

    for (ProfileNode *node : added_nodes) {
      this->find_nodes_to_draw(node->direct_children(), r_nodes);
      for (Span<ProfileNode *> children : node->parallel_children()) {
        this->find_nodes_to_draw(children, r_nodes);
      }
    }
  }

  void draw_nodes(Span<ProfileNode *> nodes)
  {
    BLI_PROFILE_SCOPE(__func__);
    uiBlock *ui_block = UI_block_begin(C, region_, __func__, UI_EMBOSS_NONE);
    for (ProfileNode *node : nodes) {
      this->draw_node(*node, ui_block);
    }
    UI_block_end(C, ui_block);
    UI_block_draw(C, ui_block);
  }

  void draw_node(ProfileNode &node, uiBlock *ui_block)
  {
    const float left_x = this->time_to_x(node.begin_time());
    const float real_right_x = this->time_to_x(node.end_time());
    const float right_x = std::max(left_x + 1, real_right_x);

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    const Color4f color = this->get_node_color(node);
    immUniformColor4fv(color);

    const float top_y = this->node_y_to_region_y(node.top_y);
    immRecti(pos, left_x, top_y, right_x, top_y - UI_UNIT_Y);

    immUnbindProgram();

    if (right_x - left_x > 1.0f) {
      this->draw_node_label(node, ui_block, left_x, right_x);
    }
  }

  struct NodeTooltipArg {
    ProfileNode *node;
  };

  void draw_node_label(ProfileNode &node, uiBlock *ui_block, const int left_x, const int right_x)
  {
    const int x = std::max(0, left_x);
    const int width = std::max(1, std::min<int>(right_x, region_->winx) - x);
    const float top_y = this->node_y_to_region_y(node.top_y);

    uiBut *but = uiDefIconTextBut(ui_block,
                                  UI_BTYPE_BUT,
                                  0,
                                  ICON_NONE,
                                  node.name().c_str(),
                                  x,
                                  top_y - UI_UNIT_Y,
                                  width,
                                  UI_UNIT_Y,
                                  nullptr,
                                  0,
                                  0,
                                  0,
                                  0,
                                  nullptr);
    UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
    UI_but_drawflag_disable(but, UI_BUT_TEXT_RIGHT);

    UI_but_func_tooltip_set(but,
                            node_tooltip_fn,
                            new (MEM_mallocN(sizeof(NodeTooltipArg), __func__))
                                NodeTooltipArg{&node});
    UI_but_func_set(but, node_click_fn, &node, profiler_layout_);
  }

  static char *node_tooltip_fn(bContext *UNUSED(C), void *argN, const char *UNUSED(tip))
  {
    NodeTooltipArg &arg = *(NodeTooltipArg *)argN;
    ProfileNode &node = *arg.node;
    const Duration duration = node.duration();
    std::stringstream ss;
    ss << std::setprecision(2) << std::fixed;
    for (const ProfileNode *parent = node.parent(); parent != nullptr; parent = parent->parent()) {
      const Duration parent_duration = parent->duration();
      const float percentage = (duration.count() / (float)parent_duration.count()) * 100.0f;
      ss << percentage << "% of " << parent->name() << "\n";
    }
    ss << "\n";
    ss << "Duration: " << duration_to_ms(duration) << " ms";
    return BLI_strdup(ss.str().c_str());
  }

  static void node_click_fn(bContext *C, void *arg1, void *arg2)
  {
    ProfileNode &node = *(ProfileNode *)arg1;
    ProfilerLayout &profiler_layout = *(ProfilerLayout *)arg2;

    ARegion *region = CTX_wm_region(C);

    const TimePoint begin_time = profiler_layout.begin_time();
    const float left_ms = duration_to_ms(node.begin_time() - begin_time);
    const float right_ms = duration_to_ms(node.end_time() - begin_time);
    const float duration_ms = right_ms - left_ms;
    const float padding = duration_ms * 0.05f;

    rctf new_view;
    BLI_rctf_init(&new_view, left_ms - padding, right_ms + padding, -1, 0);

    UI_view2d_smooth_view(C, region, &new_view, U.smooth_viewtx);
  }

  float time_to_x(const TimePoint time) const
  {
    const TimePoint begin_time = profiler_layout_->begin_time();
    const Duration time_since_begin = time - begin_time;
    const float ms_since_begin = duration_to_ms(time_since_begin);
    return UI_view2d_view_to_region_x(&region_->v2d, ms_since_begin);
  }

  TimePoint x_to_time(const float x) const
  {
    const float ms_since_begin = UI_view2d_region_to_view_x(&region_->v2d, x);
    const TimePoint begin_time = profiler_layout_->begin_time();
    return begin_time + ms_to_duration(ms_since_begin);
  }

  float node_y_to_region_y(const float y) const
  {
    return region_->winy + y * UI_UNIT_Y - region_->v2d.cur.ymax;
  }

  Color4f get_node_color(ProfileNode &node)
  {
    const uint64_t value = POINTER_AS_UINT(&node);
    const float variation = BLI_hash_int_2d_to_float(value, value >> 32);
    float r, g, b;
    hsv_to_rgb(variation * 0.2f, 0.5f, 0.5f, &r, &g, &b);
    return {r, g, b, 1.0f};
  }

  static float duration_to_ms(const Duration duration)
  {
    return duration.count() / 1000000.0f;
  }

  static Duration ms_to_duration(const float ms)
  {
    return std::chrono::nanoseconds((int64_t)(ms * 1'000'000.0f));
  }
};

void draw_profiler(const bContext *C, ARegion *region)
{
  BLI_PROFILE_SCOPE(__func__);
  ProfilerDrawer drawer{C, region};
  drawer.draw();
}

}  // namespace blender::ed::profiler
