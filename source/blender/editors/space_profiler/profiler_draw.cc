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
  int row_height_;
  int parallel_padding_;
  uiBlock *ui_block_ = nullptr;

 public:
  ProfilerDrawer(const bContext *C, ARegion *region) : C(C), region_(region)
  {
    sprofiler_ = CTX_wm_space_profiler(C);
    runtime_ = sprofiler_->runtime;

    if (!runtime_->profiler_layout) {
      runtime_->profiler_layout = std::make_unique<ProfilerLayout>();
    }
    profile::ProfileListener::flush_to_all();
    profiler_layout_ = runtime_->profiler_layout.get();

    row_height_ = UI_UNIT_Y;
    parallel_padding_ = UI_UNIT_Y * 0.2f;
  }

  void draw()
  {
    UI_ThemeClearColor(TH_BACK);

    this->compute_vertical_extends_of_all_nodes();

    ui_block_ = UI_block_begin(C, region_, __func__, UI_EMBOSS_NONE);
    this->draw_all_nodes();
    UI_block_end(C, ui_block_);
    UI_block_draw(C, ui_block_);

    this->update_view2d_bounds();
  }

  void update_view2d_bounds()
  {
    const float duration_ms = duration_to_ms(profiler_layout_->end_time() -
                                             profiler_layout_->begin_time());
    /* Giving a bit more space on the right side is convenient. */
    const float extended_duration_ms = std::max(duration_ms * 1.1f, 5000.0f);
    UI_view2d_totRect_set(&region_->v2d, extended_duration_ms, 1);

    UI_view2d_scrollers_draw(&region_->v2d, nullptr);
  }

  void compute_vertical_extends_of_all_nodes()
  {
    int top_y = region_->winy;
    for (Span<ProfileNode *> nodes : profiler_layout_->root_nodes()) {
      top_y = this->compute_vertical_extends_of_nodes(nodes, top_y);
      top_y -= parallel_padding_;
    }
  }

  float compute_vertical_extends_of_nodes(Span<ProfileNode *> nodes, const float top_y)
  {
    int bottom_y = top_y;
    for (ProfileNode *node : nodes) {
      node->top_y = top_y;
      this->compute_vertical_extends_of_node(*node);
      bottom_y = std::min(bottom_y, node->bottom_y);
    }
    return bottom_y;
  }

  void compute_vertical_extends_of_node(ProfileNode &node)
  {
    node.bottom_y = node.top_y - row_height_;
    node.bottom_y = this->compute_vertical_extends_of_nodes(node.direct_children(), node.bottom_y);
    for (Span<ProfileNode *> children : node.parallel_children()) {
      if (!children.is_empty()) {
        node.bottom_y -= parallel_padding_;
        node.bottom_y = this->compute_vertical_extends_of_nodes(children, node.bottom_y);
      }
    }
  }

  void draw_all_nodes()
  {
    for (Span<ProfileNode *> nodes : profiler_layout_->root_nodes()) {
      this->draw_nodes(nodes);
    }
  }

  void draw_nodes(Span<ProfileNode *> nodes)
  {
    for (ProfileNode *node : nodes) {
      this->draw_node(*node);
    }
  }

  void draw_node(ProfileNode &node)
  {
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    const Color4f color = this->get_node_color(node);
    immUniformColor4fv(color);

    const int left_x = this->time_to_x(node.begin_time());
    const int right_x = std::max(left_x + 1, this->time_to_x(node.end_time()));
    immRecti(pos, left_x, node.top_y, right_x, node.top_y - row_height_);

    immUnbindProgram();

    this->draw_node_label(node, left_x, right_x);

    this->draw_nodes(node.direct_children());
    for (Span<ProfileNode *> nodes : node.parallel_children()) {
      this->draw_nodes(nodes);
    }
  }

  struct NodeTooltipArg {
    ProfileNode *node;
  };

  void draw_node_label(ProfileNode &node, const int left_x, const int right_x)
  {
    const int x = std::max(0, left_x);
    const int width = std::max(1, std::min<int>(right_x, region_->winx) - x);

    uiBut *but = uiDefIconTextBut(ui_block_,
                                  UI_BTYPE_BUT,
                                  0,
                                  ICON_NONE,
                                  node.name().c_str(),
                                  x,
                                  node.top_y - row_height_,
                                  width,
                                  row_height_,
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
    std::stringstream ss;
    ss << "Duration: " << duration_to_ms(node.end_time() - node.begin_time()) << " ms";
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

  int time_to_x(const TimePoint time) const
  {
    const TimePoint begin_time = profiler_layout_->begin_time();
    const Duration time_since_begin = time - begin_time;
    const float ms_since_begin = duration_to_ms(time_since_begin);
    return UI_view2d_view_to_region_x(&region_->v2d, ms_since_begin);
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
};

void draw_profiler(const bContext *C, ARegion *region)
{
  ProfilerDrawer drawer{C, region};
  drawer.draw();
}

}  // namespace blender::ed::profiler
