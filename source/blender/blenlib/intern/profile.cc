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

#include "BLI_function_ref.hh"
#include "BLI_profile.hh"
#include "BLI_profile_manage.hh"
#include "BLI_single_producer_chunk_consumer.hh"
#include "BLI_stack.hh"

using namespace blender;
using namespace blender::profile;

static uint64_t get_unique_session_id()
{
  /* TODO: Allow getting ids without synchronizing threads for every id. */
  static std::atomic<uint64_t> id = 1;
  return id++;
}

struct ThreadLocalProfileData;

static std::mutex registered_threadlocals_mutex;
static RawVector<ThreadLocalProfileData *> registered_threadlocals;

template<typename T> using ProfileDataQueue = SingleProducerChunkConsumerQueue<T, RawAllocator>;

struct ThreadLocalProfileData {
  ThreadLocalProfileData()
  {
    std::lock_guard lock{registered_threadlocals_mutex};
    registered_threadlocals.append(this);
    thread_id = get_unique_session_id();
  }

  ~ThreadLocalProfileData()
  {
    std::lock_guard lock{registered_threadlocals_mutex};
    registered_threadlocals.remove_first_occurrence_and_reorder(this);
  }

  uint64_t thread_id;
  ProfileDataQueue<ProfileTaskBegin> queue_begins;
  ProfileDataQueue<ProfileTaskEnd> queue_ends;
  RawStack<uint64_t> id_stack;
};

static ThreadLocalProfileData threadlocal_profile_data;
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

static std::mutex listeners_mutex;
static RawVector<ProfileListener *> listeners;

ProfileListener::ProfileListener()
{
  std::lock_guard lock{listeners_mutex};
  listeners.append(this);
  if (listeners.size() == 1) {
    start_profiling();
  }
}

ProfileListener::~ProfileListener()
{
  std::lock_guard lock{listeners_mutex};
  listeners.remove_first_occurrence_and_reorder(this);
  if (listeners.is_empty()) {
    stop_profiling();
  }
}

void ProfileListener::flush_to_all()
{
  /* Todo: How to handle short lived threads? */
  std::scoped_lock lock{listeners_mutex, registered_threadlocals_mutex};
  for (ThreadLocalProfileData *data : registered_threadlocals) {
    RecordedProfile recorded_profile;
    data->queue_begins.consume(
        [&](Span<ProfileTaskBegin> data) { recorded_profile.task_begins.extend(data); });
    data->queue_ends.consume(
        [&](Span<ProfileTaskEnd> data) { recorded_profile.task_ends.extend(data); });
    for (ProfileListener *listener : listeners) {
      listener->handle(recorded_profile);
    }
  }
}

}  // namespace blender::profile

void _bli_profile_task_begin(BLI_ProfileTask *task, const char *name)
{
  task->id = get_unique_session_id();

  ProfileTaskBegin *task_begin = threadlocal_profile_data.queue_begins.prepare_append();
  task_begin->id = task->id;
  task_begin->name = name;
  task_begin->parent_id = threadlocal_profile_data.id_stack.peek_default(0);
  task_begin->thread_id = threadlocal_profile_data.thread_id;
  task_begin->time = Clock::now();

  threadlocal_profile_data.queue_begins.commit_append();
}

void _bli_profile_task_begin_subtask(BLI_ProfileTask *task,
                                     const char *name,
                                     const BLI_ProfileTask *parent_task)
{
  task->id = get_unique_session_id();

  ProfileTaskBegin *task_begin = threadlocal_profile_data.queue_begins.prepare_append();
  task_begin->id = task->id;
  task_begin->name = name;
  task_begin->parent_id = parent_task->id;
  task_begin->thread_id = threadlocal_profile_data.thread_id;
  task_begin->time = Clock::now();

  threadlocal_profile_data.queue_begins.commit_append();
}

void _bli_profile_task_end(BLI_ProfileTask *task)
{
  TimePoint time = Clock::now();

  ProfileTaskEnd *task_end = threadlocal_profile_data.queue_ends.prepare_append();
  task_end->begin_id = task->id;
  task_end->time = time;

  threadlocal_profile_data.queue_ends.commit_append();
}
