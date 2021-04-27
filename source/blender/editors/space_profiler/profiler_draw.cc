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

#include "BLI_rect.h"

#include "profiler_draw.hh"
#include "profiler_layout.hh"
#include "profiler_runtime.hh"

namespace blender::ed::profiler {

void draw_profiler(const bContext *C, ARegion *region)
{
  SpaceProfiler *sprofiler = CTX_wm_space_profiler(C);
  SpaceProfiler_Runtime &runtime = *sprofiler->runtime;

  if (!runtime.profiler_layout) {
    runtime.profiler_layout = std::make_unique<ProfilerLayout>();
  }
  profile::ProfileListener::flush_to_all();
  ProfilerLayout &profiler_layout = *runtime.profiler_layout;

  UI_ThemeClearColor(TH_BACK);
}

}  // namespace blender::ed::profiler
