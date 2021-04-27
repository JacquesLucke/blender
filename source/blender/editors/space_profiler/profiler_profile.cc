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

#include "ED_profiler.h"

#include "DNA_space_types.h"

#include "profiler_runtime.hh"

void ED_profiler_profile_enable(SpaceProfiler *sprofiler)
{
  if (ED_profiler_profile_is_enabled(sprofiler)) {
    return;
  }
  SpaceProfiler_Runtime &runtime = *sprofiler->runtime;
  runtime.profile_listener = std::make_unique<blender::ed::profiler::SpaceProfilerListener>(
      runtime);
}

void ED_profiler_profile_disable(SpaceProfiler *sprofiler)
{
  if (!ED_profiler_profile_is_enabled(sprofiler)) {
    return;
  }
  SpaceProfiler_Runtime &runtime = *sprofiler->runtime;
  runtime.profile_listener.reset();
}

bool ED_profiler_profile_is_enabled(SpaceProfiler *sprofiler)
{
  SpaceProfiler_Runtime &runtime = *sprofiler->runtime;
  return (bool)runtime.profile_listener;
}

void ED_profiler_profile_clear(SpaceProfiler *sprofiler)
{
  SpaceProfiler_Runtime &runtime = *sprofiler->runtime;
  runtime.profiler_layout.reset();
}
