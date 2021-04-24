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

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender::profile {

using Clock = std::chrono::steady_clock;
using Duration = Clock::duration;
using TimePoint = Clock::time_point;
using Nanoseconds = std::chrono::nanoseconds;

struct ProfileSegment {
  const char *name;
  TimePoint begin_time;
  TimePoint end_time;
  uint64_t id;
  uint64_t parent_id;
  uint64_t thread_id;
};

class ProfileResult;

class ProfileNode : NonCopyable, NonMovable {
 private:
  ProfileNode *parent_ = nullptr;
  const char *name_ = nullptr;
  TimePoint begin_time_;
  TimePoint end_time_;
  int thread_id_;
  Vector<ProfileNode *> children_;

  friend ProfileResult;

 public:
  const ProfileNode *parent() const
  {
    return parent_;
  }

  StringRefNull name() const
  {
    return name_;
  }

  TimePoint begin_time() const
  {
    return begin_time_;
  }

  TimePoint end_time() const
  {
    return end_time_;
  }

  int thread_id() const
  {
    return thread_id_;
  }

  Span<const ProfileNode *> children() const
  {
    return children_;
  }

 private:
  void destruct_recursively();
};

class ProfileResult {
 private:
  LinearAllocator<> allocator_;
  Vector<ProfileNode *> root_nodes_;
  Map<uint64_t, ProfileNode *> nodes_by_id_;
  TimePoint begin_time_;
  TimePoint end_time_;

 public:
  ProfileResult() = default;
  ~ProfileResult();

  void add(Span<ProfileSegment> segments);

  TimePoint begin_time() const
  {
    return begin_time_;
  }

  TimePoint end_time() const
  {
    return end_time_;
  }

  Span<const ProfileNode *> root_nodes() const
  {
    return root_nodes_;
  }
};

Vector<ProfileSegment> get_recorded_segments();

}  // namespace blender::profile
