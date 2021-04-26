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

#include <mutex>

#include "BLI_profile.hh"
#include "BLI_profile_manage.hh"

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
  std::lock_guard lock{listeners_mutex};
  RecordedProfile recorded_profile;

  for (ProfileListener *listener : listeners) {
    listener->handle(recorded_profile);
  }
}

}  // namespace blender::profile

using namespace blender::profile;

void _bli_profile_task_begin(BLI_ProfileTask *task, const char *name)
{
  UNUSED_VARS(task, name);
}

void _bli_profile_task_begin_subtask(BLI_ProfileTask *task,
                                     const char *name,
                                     const BLI_ProfileTask *parent_task)
{
  UNUSED_VARS(task, name, parent_task);
}

void _bli_profile_task_end(BLI_ProfileTask *task)
{
  UNUSED_VARS(task);
}
