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
#include "BLI_profile_manage.hh"
#include "BLI_vector_set.hh"

namespace blender::ed::profiler {

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
  /* The nodes in these vectors are ordered by the begin time. Nodes in a single vector should not
   * overlap. */
  Vector<ProfileNode *> direct_children_;
  Vector<Vector<ProfileNode *>> parallel_children_;

  /* These nodes still have to be inserted into the vectors above. They are not sorted. */
  Vector<ProfileNode *> children_to_pack_;

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
    return direct_children_;
  }

  Span<Vector<ProfileNode *>> stacked_children_in_other_threads() const
  {
    return parallel_children_;
  }

  static bool time_overlap(const ProfileNode &a, const ProfileNode &b);

 private:
  void pack_added_children();
  int try_pack_into_vector(Vector<ProfileNode *> &nodes_vec, bool ignore_other_thread_ids);

  void destruct_recursively();
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
  ~ProfileLayout();

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

}  // namespace blender::ed::profiler
