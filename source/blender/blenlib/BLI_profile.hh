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

#include "BLI_profile.h"
#include "BLI_vector.hh"

namespace blender::profile {

using Clock = std::chrono::steady_clock;
using Duration = Clock::duration;
using TimePoint = Clock::time_point;
using Nanoseconds = std::chrono::nanoseconds;

struct ProfileSegmentBegin {
  std::string name;
  TimePoint time;
  uint64_t id;
  uint64_t parent_id;
  uint64_t thread_id;
};

struct ProfileSegmentEnd {
  TimePoint time;
  uint64_t begin_id;
};

struct RecordedProfile {
  Vector<ProfileSegmentBegin> begins;
  Vector<ProfileSegmentEnd> ends;
};

using ProfileListenerFn = std::function<void(const RecordedProfile &)>;
uint64_t register_listener(ProfileListenerFn listener_fn);
void unregister_listener(uint64_t listener_handle);
void flush_to_listeners();

class ProfileScope {
 private:
  BLI_profile_scope scope;

 public:
  ProfileScope(const char *name)
  {
    BLI_profile_scope_begin(&scope, name);
  }

  ~ProfileScope()
  {
    BLI_profile_scope_end(&scope);
  }
};

}  // namespace blender::profile

#define BLI_SCOPED_PROFILE(name) blender::profile::ProfileScope profile_scope(name)
