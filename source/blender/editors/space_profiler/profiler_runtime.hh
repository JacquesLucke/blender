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

#pragma once

#include <optional>

#include "profiler_layout.hh"

#include "DNA_space_types.h"

namespace blender::ed::profiler {

class SpaceProfilerListener : public profile::ProfileListener {
 private:
  SpaceProfiler_Runtime *runtime_;

 public:
  SpaceProfilerListener(SpaceProfiler_Runtime &runtime);

  void handle(const RecordedProfile &profile) final;
};

}  // namespace blender::ed::profiler

struct SpaceProfiler_Runtime {
  std::unique_ptr<blender::ed::profiler::ProfileLayout> profile_layout;
  std::unique_ptr<blender::ed::profiler::SpaceProfilerListener> profile_listener;

  SpaceProfiler_Runtime() = default;
  SpaceProfiler_Runtime(const SpaceProfiler_Runtime &UNUSED(other))
  {
  }
};
