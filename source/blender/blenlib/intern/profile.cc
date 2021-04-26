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
#include <mutex>

#include "BLI_profile.hh"
#include "BLI_profile_manage.hh"
#include "BLI_stack.hh"

using namespace blender;
using namespace blender::profile;

static uint64_t get_unique_session_id()
{
  /* TODO: Allow getting ids without synchronizing threads for every id. */
  static std::atomic<uint64_t> id = 1;
  return id++;
}

static RawVector<ProfileTaskBegin> recorded_task_begins;
static RawVector<ProfileTaskEnd> recorded_task_ends;
static thread_local RawStack<uint64_t> threadlocal_id_stack;
static thread_local uint64_t threadlocal_thread_id = get_unique_session_id();
bool bli_profiling_is_enabled = false;

namespace blender::profile {

static void start_profiling()
{
  bli_profiling_is_enabled = true;
}

static void stop_profiling()
{
  bli_profiling_is_enabled = false;
}

/* TODO: Need to reduce threading overhead, but this works fine for now. */
static std::mutex profile_mutex;
static RawVector<ProfileListener *> listeners;

ProfileListener::ProfileListener()
{
  std::lock_guard lock{profile_mutex};
  listeners.append(this);
  if (listeners.size() == 1) {
    start_profiling();
  }
}

ProfileListener::~ProfileListener()
{
  std::lock_guard lock{profile_mutex};
  listeners.remove_first_occurrence_and_reorder(this);
  if (listeners.is_empty()) {
    stop_profiling();
  }
}

void ProfileListener::flush_to_all()
{
  std::lock_guard lock{profile_mutex};
  RecordedProfile recorded_profile;
  recorded_profile.task_begins = std::move(recorded_task_begins);
  recorded_profile.task_ends = std::move(recorded_task_ends);

  for (ProfileListener *listener : listeners) {
    listener->handle(recorded_profile);
  }
}

}  // namespace blender::profile

void _bli_profile_task_begin(BLI_ProfileTask *task, const char *name)
{
  ProfileTaskBegin task_begin;
  task_begin.id = get_unique_session_id();
  task_begin.name = name;
  task_begin.parent_id = threadlocal_id_stack.peek_default(0);
  task_begin.thread_id = threadlocal_thread_id;
  task_begin.time = Clock::now();

  task->id = task_begin.id;

  std::scoped_lock lock{profile_mutex};
  recorded_task_begins.append(task_begin);
}

void _bli_profile_task_begin_subtask(BLI_ProfileTask *task,
                                     const char *name,
                                     const BLI_ProfileTask *parent_task)
{
  ProfileTaskBegin task_begin;
  task_begin.id = get_unique_session_id();
  task_begin.name = name;
  task_begin.parent_id = parent_task->id;
  task_begin.thread_id = threadlocal_thread_id;
  task_begin.time = Clock::now();

  task->id = task_begin.id;

  std::scoped_lock lock{profile_mutex};
  recorded_task_begins.append(task_begin);
}

void _bli_profile_task_end(BLI_ProfileTask *task)
{
  ProfileTaskEnd task_end;
  task_end.begin_id = task->id;
  task_end.time = Clock::now();

  std::scoped_lock lock{profile_mutex};
  recorded_task_ends.append(task_end);
}
