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

#include <chrono>
#include <string>

#include "BLI_vector.hh"

namespace blender::profile {

using Clock = std::chrono::steady_clock;
using Duration = Clock::duration;
using TimePoint = Clock::time_point;
using Nanoseconds = std::chrono::nanoseconds;

struct ProfileTaskBegin {
  TimePoint time;
  uint64_t id;
  uint64_t parent_id;
  uint64_t thread_id;
};

struct ProfileTaskBeginNamed : public ProfileTaskBegin {
  const char *name;
};

struct ProfileTaskBeginRange : public ProfileTaskBegin {
  int64_t start;
  int64_t one_after_last;
};

struct ProfileTaskEnd {
  TimePoint time;
  uint64_t begin_id;
};

struct RecordedProfile {
  RawVector<ProfileTaskBeginNamed> task_begins_named;
  RawVector<ProfileTaskBeginRange> task_begins_range;
  RawVector<ProfileTaskEnd> task_ends;
};

class ProfileListener {
 public:
  ProfileListener();
  virtual ~ProfileListener();

  virtual void handle(const RecordedProfile &profile) = 0;

  static void flush_to_all();
};

}  // namespace blender::profile
