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
    this->draw_all_nodes();
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
      node.bottom_y -= parallel_padding_;
      node.bottom_y = this->compute_vertical_extends_of_nodes(children, node.bottom_y);
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
    immRecti(pos, left_x, node.top_y, right_x, node.bottom_y);

    immUnbindProgram();
  }

  int time_to_x(const TimePoint time) const
  {
    const TimePoint begin_time = profiler_layout_->begin_time();
    const Duration time_since_begin = time - begin_time;
    const float ms_since_begin = this->duration_to_ms(time_since_begin);
    return ms_since_begin / 5.0f;
  }

  Color4f get_node_color(ProfileNode &node)
  {
    const uint64_t value = node.begin_time().time_since_epoch().count();
    const float variation = BLI_hash_int_2d_to_float(value, value >> 32);
    float r, g, b;
    hsv_to_rgb(variation * 0.2f, 0.5f, 0.5f, &r, &g, &b);
    return {r, g, b, 1.0f};
  }

  float duration_to_ms(const Duration duration) const
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
