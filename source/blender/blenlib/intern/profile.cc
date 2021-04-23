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

#include <atomic>
#include <chrono>
#include <mutex>

#include "BLI_profile.h"
#include "BLI_profile_parse.hh"
#include "BLI_stack.hh"
#include "BLI_vector.hh"

namespace blender::profile {

struct ProfileSegment {
  const char *name;
  TimePoint start_time;
  TimePoint end_time;
  uint64_t id;
  uint64_t parent_id;
};

static std::mutex profile_mutex;
static Vector<ProfileSegment> segments;
static bool profiling_is_enabled;

struct ThreadLocalProfileStorage {
  Stack<uint64_t, 0, RawAllocator> scope_stack;
  Vector<ProfileSegment, 0, RawAllocator> segments;

  void add_segment(ProfileSegment segment)
  {
    std::lock_guard lock{profile_mutex};
    if (profiling_is_enabled) {
      segments.append(segment);
    }
  }
};

static thread_local ThreadLocalProfileStorage storage;

static uint64_t get_unique_session_id()
{
  /* TODO: Allow getting ids without synchronizing threads for every id. */
  static std::atomic<uint64_t> id = 1;
  return id++;
}

}  // namespace blender::profile

using namespace blender::profile;

void BLI_profile_scope_begin(BLI_profile_scope *scope, const char *name)
{
  scope->name = name;
  scope->start_time = Clock::now().time_since_epoch().count();
  scope->id = get_unique_session_id();
  scope->parent_id = storage.scope_stack.peek_default(0);
  storage.scope_stack.push(scope->id);
}

void BLI_profile_scope_begin_subthread(BLI_profile_scope *scope,
                                       const BLI_profile_scope *parent_scope,
                                       const char *name)
{
  scope->name = name;
  scope->id = get_unique_session_id();
  scope->parent_id = parent_scope->id;
  scope->start_time = Clock::now().time_since_epoch().count();
  storage.scope_stack.push(scope->id);
}

void BLI_profile_scope_end(const BLI_profile_scope *scope)
{
  TimePoint end_time = Clock::now();
  BLI_assert(storage.scope_stack.peek() == scope->id);
  storage.scope_stack.pop();
  TimePoint start_time = TimePoint(Duration(scope->start_time));

  ProfileSegment segment;
  segment.name = scope->name;
  segment.start_time = start_time;
  segment.end_time = end_time;
  segment.id = scope->id;
  segment.parent_id = scope->parent_id;
  storage.add_segment(segment);
}

void BLI_profile_enable()
{
  std::lock_guard lock{profile_mutex};
  profiling_is_enabled = true;
}

void BLI_profile_disable()
{
  std::lock_guard lock{profile_mutex};
  profiling_is_enabled = false;
}

void BLI_profile_clear()
{
  std::lock_guard lock{profile_mutex};
  segments.clear();
}
