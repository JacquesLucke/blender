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

#include "BLI_map.hh"
#include "BLI_profile.hh"
#include "BLI_stack.hh"
#include "BLI_vector.hh"

namespace blender::profile {

static std::mutex profile_mutex;
static Vector<ProfileSegmentBegin, 0, RawAllocator> recorded_begins;
static Vector<ProfileSegmentEnd, 0, RawAllocator> recorded_ends;
static bool profiling_is_enabled;

static uint64_t get_unique_session_id()
{
  /* TODO: Allow getting ids without synchronizing threads for every id. */
  static std::atomic<uint64_t> id = 1;
  return id++;
}

struct ThreadLocalProfileStorage {
  uint64_t thread_id;
  Stack<uint64_t, 0, RawAllocator> scope_stack;

  ThreadLocalProfileStorage()
  {
    thread_id = get_unique_session_id();
  }

  void add_begin(const char *name,
                 const TimePoint time,
                 const uint64_t id,
                 const uint64_t parent_id)
  {
    std::lock_guard lock{profile_mutex};
    if (profiling_is_enabled) {
      recorded_begins.append({name, time, id, parent_id, this->thread_id});
    }
  }

  void add_end(const TimePoint time, const uint64_t begin_id)
  {
    std::lock_guard lock{profile_mutex};
    if (profiling_is_enabled) {
      recorded_ends.append({time, begin_id});
    }
  }
};

static thread_local ThreadLocalProfileStorage storage;

static RecordedProfile extract_recorded_profile()
{
  std::lock_guard lock{profile_mutex};
  RecordedProfile recorded_profile{recorded_begins, recorded_ends};
  recorded_begins.clear();
  recorded_ends.clear();
  return recorded_profile;
}

using ListenerMap = Map<uint64_t, ProfileListenerFn>;

static std::atomic<uint64_t> next_listener_handle = 0;

static ListenerMap &get_listener_map()
{
  static ListenerMap listener_map;
  return listener_map;
}

uint64_t register_listener(ProfileListenerFn listener_fn)
{
  const uint64_t handle = next_listener_handle.fetch_add(1);
  ListenerMap &map = get_listener_map();
  map.add_new(handle, listener_fn);

  BLI_profile_enable();

  return handle;
}

void unregister_listener(const uint64_t listener_handle)
{
  ListenerMap &map = get_listener_map();
  map.remove(listener_handle);

  if (map.is_empty()) {
    BLI_profile_disable();
  }
}

void flush_to_listeners()
{
  RecordedProfile recorded_profile = blender::profile::extract_recorded_profile();
  const ListenerMap &map = get_listener_map();
  for (const ProfileListenerFn &fn : map.values()) {
    fn(recorded_profile);
  }
}

}  // namespace blender::profile

using namespace blender::profile;

void BLI_profile_scope_begin(BLI_profile_scope *scope, const char *name)
{
  const uint64_t id = get_unique_session_id();
  scope->id = id;
  const uint64_t parent_id = storage.scope_stack.peek_default(0);
  storage.scope_stack.push(id);
  const TimePoint time = Clock::now();
  storage.add_begin(name, time, id, parent_id);
}

void BLI_profile_scope_begin_subthread(BLI_profile_scope *scope,
                                       const BLI_profile_scope *parent_scope,
                                       const char *name)
{
  const uint64_t id = get_unique_session_id();
  scope->id = id;
  const uint64_t parent_id = parent_scope->id;
  storage.scope_stack.push(id);
  const TimePoint time = Clock::now();
  storage.add_begin(name, time, id, parent_id);
}

void BLI_profile_scope_end(const BLI_profile_scope *scope)
{
  TimePoint time = Clock::now();
  BLI_assert(storage.scope_stack.peek() == scope->id);
  storage.scope_stack.pop();
  storage.add_end(time, scope->id);
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
  recorded_begins.clear();
  recorded_ends.clear();
}

bool BLI_profile_is_enabled()
{
  return profiling_is_enabled;
}
