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
#include "BLI_linear_allocator.hh"
#include "BLI_profile.hh"
#include "BLI_profile_manage.hh"
#include "BLI_single_producer_chunk_consumer.hh"
#include "BLI_stack.hh"

using namespace blender;
using namespace blender::profile;

static uint64_t get_unique_session_id_range(const uint64_t size)
{
  static std::atomic<uint64_t> id = 1;
  const uint64_t range_start = id.fetch_add(size, std::memory_order_relaxed);
  return range_start;
}

struct ThreadLocalProfileData;

struct ProfileRegistry {
  static inline std::mutex threadlocals_mutex;
  RawVector<ThreadLocalProfileData *> threadlocals;

  static inline std::mutex listeners_mutex;
  RawVector<ProfileListener *> listeners;
};

/**
 * All threads that record profile data register themselves here.
 * It is a shared pointer, because the individual threadlocal variables have to own the registry as
 * well. Otherwise there are problems at shutdown when this static variable is destructed before
 * all other threads unregistered themselves.
 */
static std::shared_ptr<ProfileRegistry> registry;

static std::shared_ptr<ProfileRegistry> &ensure_registry()
{
  static std::mutex mutex;
  std::lock_guard lock{mutex};
  if (!registry) {
    registry = std::make_shared<ProfileRegistry>();
  }
  return registry;
}

template<typename T, typename UserData = void>
using ProfileDataQueue = SingleProducerChunkConsumerQueue<T, RawAllocator, UserData>;

struct ThreadLocalProfileData {
  ThreadLocalProfileData()
  {
    std::lock_guard lock{ProfileRegistry::threadlocals_mutex};
    used_registry = ensure_registry();
    registry->threadlocals.append(this);
    thread_id = get_unique_session_id_range(1);
  }

  ~ThreadLocalProfileData()
  {
    std::lock_guard lock{ProfileRegistry::threadlocals_mutex};
    used_registry->threadlocals.remove_first_occurrence_and_reorder(this);
  }

  uint64_t thread_id;
  ProfileDataQueue<ProfileTaskBeginNamed, LinearAllocator<RawAllocator>> queue_begins_named;
  ProfileDataQueue<ProfileTaskBeginRange> queue_begins_range;
  ProfileDataQueue<ProfileTaskEnd> queue_ends;
  RawStack<uint64_t> id_stack;

  uint64_t get_next_unique_id()
  {
    if (unique_id_current_ == unique_id_end_) {
      constexpr uint64_t size = 100'000;
      unique_id_current_ = get_unique_session_id_range(size);
      unique_id_end_ = unique_id_current_ + size;
    }
    const uint64_t id = unique_id_current_;
    unique_id_current_++;
    return id;
  }

  /* Take ownership to make sure that the registry won't be destructed too early. */
  std::shared_ptr<ProfileRegistry> used_registry;

 private:
  uint64_t unique_id_current_ = 0;
  uint64_t unique_id_end_ = 0;
};

static thread_local ThreadLocalProfileData threadlocal_profile_data;
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

ProfileListener::ProfileListener()
{
  std::lock_guard lock{ProfileRegistry::listeners_mutex};
  ensure_registry();
  registry->listeners.append(this);
  if (registry->listeners.size() == 1) {
    start_profiling();
  }
}

ProfileListener::~ProfileListener()
{
  std::lock_guard lock{ProfileRegistry::listeners_mutex};
  ensure_registry();
  registry->listeners.remove_first_occurrence_and_reorder(this);
  if (registry->listeners.is_empty()) {
    stop_profiling();
  }
}

void ProfileListener::flush_to_all()
{
  /* Todo: How to handle short lived threads? */
  std::scoped_lock lock{ProfileRegistry::threadlocals_mutex, ProfileRegistry::listeners_mutex};
  if (!registry) {
    return;
  }
  RecordedProfile recorded_profile;
  for (ThreadLocalProfileData *data : registry->threadlocals) {
    data->queue_begins_named.consume([&](Span<ProfileTaskBeginNamed> data) {
      recorded_profile.task_begins_named.extend(data);
    });
    data->queue_begins_range.consume([&](Span<ProfileTaskBeginRange> data) {
      recorded_profile.task_begins_range.extend(data);
    });
    data->queue_ends.consume(
        [&](Span<ProfileTaskEnd> data) { recorded_profile.task_ends.extend(data); });
  }
  for (ProfileListener *listener : registry->listeners) {
    listener->handle(recorded_profile);
  }
}

}  // namespace blender::profile

static void profile_task_begin_named(BLI_ProfileTask *task, const char *name, uint64_t parent_id)
{
  ThreadLocalProfileData &local_data = threadlocal_profile_data;

  const uint64_t id = local_data.get_next_unique_id();
  local_data.id_stack.push(id);
  task->id = id;

  ProfileTaskBeginNamed *task_begin = local_data.queue_begins_named.prepare_append();
  LinearAllocator<RawAllocator> *allocator =
      local_data.queue_begins_named.user_data_for_current_append();
  StringRefNull name_copy = allocator->copy_string(name);

  task_begin->id = id;
  task_begin->name = name_copy.c_str();
  task_begin->parent_id = parent_id;
  task_begin->thread_id = local_data.thread_id;
  task_begin->time = Clock::now();

  local_data.queue_begins_named.commit_append();
}

void _bli_profile_task_begin_named(BLI_ProfileTask *task, const char *name)
{
  ThreadLocalProfileData &local_data = threadlocal_profile_data;
  const uint64_t parent_id = local_data.id_stack.peek_default(0);
  profile_task_begin_named(task, name, parent_id);
}

void _bli_profile_task_begin_named_subtask(BLI_ProfileTask *task,
                                           const char *name,
                                           const BLI_ProfileTask *parent_task)
{
  profile_task_begin_named(task, name, parent_task->id);
}

void _bli_profile_task_begin_range(BLI_ProfileTask *task,
                                   const BLI_ProfileTask *parent_task,
                                   int64_t start,
                                   int64_t one_after_last)
{
  ThreadLocalProfileData &local_data = threadlocal_profile_data;

  const uint64_t id = local_data.get_next_unique_id();
  local_data.id_stack.push(id);
  task->id = id;

  ProfileTaskBeginRange *task_begin = local_data.queue_begins_range.prepare_append();
  task_begin->id = id;
  task_begin->parent_id = parent_task->id;
  task_begin->thread_id = local_data.thread_id;
  task_begin->start = start;
  task_begin->one_after_last = one_after_last;
  task_begin->time = Clock::now();

  local_data.queue_begins_range.commit_append();
}

void _bli_profile_task_end(BLI_ProfileTask *task)
{
  TimePoint time = Clock::now();

  ThreadLocalProfileData &local_data = threadlocal_profile_data;

  BLI_assert(local_data.id_stack.peek() == task->id);
  local_data.id_stack.pop();

  ProfileTaskEnd *task_end = local_data.queue_ends.prepare_append();
  task_end->begin_id = task->id;
  task_end->time = time;

  local_data.queue_ends.commit_append();
}
