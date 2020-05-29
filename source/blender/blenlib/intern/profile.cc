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

#include <chrono>
#include <iostream>
#include <list>
#include <mutex>
#include <string>

#include "BLI_array.hh"
#include "BLI_profile.h"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace BLI {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Nanoseconds = std::chrono::nanoseconds;

struct ProfileTimePoint {
  TimePoint time;
  const ProfilePosition *position;
};

class ProfileDataChunk {
 private:
  std::array<ProfileTimePoint, 1000> m_time_points;
  uint m_size = 0;
  ProfileDataChunk *m_next_in_thread = nullptr;

 public:
  void append(TimePoint time, const ProfilePosition *position)
  {
    BLI_assert(m_size < m_time_points.size());
    m_time_points[m_size] = {time, position};
    m_size++;
  }

  bool is_full() const
  {
    return m_time_points.size() == m_size;
  }

  void set_next(ProfileDataChunk *other)
  {
    m_next_in_thread = other;
  }
};

class GlobalProfilingData {
 private:
  std::mutex m_mutex;
  Vector<std::unique_ptr<ProfileDataChunk>, 0, RawAllocator> m_chunks;
  Vector<ProfileDataChunk *> m_initial_chunks;

 public:
  ProfileDataChunk *new_chunk(ProfileDataChunk *previous)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    ProfileDataChunk *chunk = new ProfileDataChunk();
    m_chunks.append(std::unique_ptr<ProfileDataChunk>(chunk));
    if (previous == nullptr) {
      m_initial_chunks.append(chunk);
    }
    else {
      previous->set_next(chunk);
    }
    return chunk;
  }
};

static GlobalProfilingData global_profiling_data;

class LocalProfilingData {
 private:
  ProfileDataChunk *m_current_chunk;

 public:
  LocalProfilingData()
  {
    m_current_chunk = global_profiling_data.new_chunk(nullptr);
  }

  void begin_scope(const ProfilePosition *position)
  {
    if (m_current_chunk->is_full()) {
      m_current_chunk = global_profiling_data.new_chunk(m_current_chunk);
    }
    m_current_chunk->append(Clock::now(), position);
  }

  void end_scope()
  {
    if (m_current_chunk->is_full()) {
      m_current_chunk = global_profiling_data.new_chunk(m_current_chunk);
    }
    m_current_chunk->append(Clock::now(), nullptr);
  }
};

thread_local LocalProfilingData local_profiling_data;

}  // namespace BLI

void bli_profile_begin(const ProfilePosition *position)
{
  BLI::local_profiling_data.begin_scope(position);
}

void bli_profile_end()
{
  BLI::local_profiling_data.end_scope();
}
