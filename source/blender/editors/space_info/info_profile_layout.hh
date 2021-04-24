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

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_profile.hh"
#include "BLI_vector_set.hh"

namespace blender::ed::info {

using profile::RecordedProfile;
using profile::TimePoint;

class ProfileLayout;

class ProfileNode {
 private:
  std::string name_;
  TimePoint begin_time_;
  TimePoint end_time_;
  ProfileNode *parent_ = nullptr;
  uint64_t id_;
  uint64_t parent_id_;
  uint64_t thread_id_;
  Vector<ProfileNode *> children_on_same_thread_;
  Vector<Vector<ProfileNode *>> packed_children_on_other_threads_;

  friend ProfileLayout;

 public:
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

  ProfileNode *parent()
  {
    return parent_;
  }

  const ProfileNode *parent() const
  {
    return parent_;
  }

  uint64_t thread_id() const
  {
    return thread_id_;
  }

  Span<const ProfileNode *> children_on_same_thread() const
  {
    return children_on_same_thread_;
  }

  static bool time_overlap(const ProfileNode &a, const ProfileNode &b);

 private:
  void add_child(ProfileNode *new_child);
};

class ProfileLayout {
 private:
  LinearAllocator<> allocator_;

  Map<uint64_t, ProfileNode *> nodes_by_id_;
  Vector<uint64_t> root_thread_ids_;
  Map<uint64_t, Vector<ProfileNode *>> root_nodes_by_thread_id_;

  TimePoint begin_time_;
  TimePoint end_time_;

 public:
  void add(const RecordedProfile &recorded);

  Span<uint64_t> root_thread_ids() const
  {
    return root_thread_ids_;
  }

  Span<ProfileNode *> root_nodes_by_thread_id(const uint64_t thread_id)
  {
    return root_nodes_by_thread_id_.lookup(thread_id);
  }

  Span<const ProfileNode *> root_nodes_by_thread_id(const uint64_t thread_id) const
  {
    return root_nodes_by_thread_id_.lookup(thread_id);
  }

  TimePoint begin_time() const
  {
    return begin_time_;
  }

  TimePoint end_time() const
  {
    return end_time_;
  }
};

}  // namespace blender::ed::info
