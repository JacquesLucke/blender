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

#include "info_profile_layout.hh"

#include "BLI_set.hh"

namespace blender::ed::info {

using profile::ProfileSegmentBegin;
using profile::ProfileSegmentEnd;

bool ProfileNode::time_overlap(const ProfileNode &a, const ProfileNode &b)
{
  const bool begin_of_a_is_in_b = (a.begin_time_ > b.begin_time_ && a.begin_time_ < b.end_time_);
  const bool begin_of_b_is_in_a = (b.begin_time_ > a.begin_time_ && b.begin_time_ < a.end_time_);
  return begin_of_a_is_in_b || begin_of_b_is_in_a;
}

void ProfileNode::pack_added_children()
{
  /* Sort new nodes by begin time. */
  std::sort(children_to_pack_.begin(),
            children_to_pack_.end(),
            [](ProfileNode *a, ProfileNode *b) { return a->begin_time() < b->begin_time(); });

  /* Assume already packed children are sorted by begin time. */
  int tot_newly_inserted = 0;
  tot_newly_inserted += this->try_pack_into_vector(children_to_pack_, true);

  int iteration = 0;
  while (tot_newly_inserted < children_to_pack_.size()) {
    if (iteration == packed_children_on_other_threads_.size()) {
      packed_children_on_other_threads_.append({});
    }
    Vector<ProfileNode *> &children_vec = packed_children_on_other_threads_[iteration];
    iteration++;
    tot_newly_inserted += this->try_pack_into_vector(children_vec, false);
  }

  children_to_pack_.clear();
}

int ProfileNode::try_pack_into_vector(Vector<ProfileNode *> &nodes_vec,
                                      bool ignore_other_thread_ids)
{
  int tot_newly_inserted = 0;
  MutableSpan<ProfileNode *> remaining_existing = nodes_vec;
  MutableSpan<ProfileNode *> remaining_new = children_to_pack_;
  Vector<ProfileNode *> new_vec;
  while (!remaining_new.is_empty()) {
    ProfileNode *new_child = remaining_new[0];
    if (new_child == nullptr) {
      /* Child has been inserted already. */
      remaining_new = remaining_new.drop_front(1);
      continue;
    }
    if (ignore_other_thread_ids) {
      if (new_child->thread_id_ != thread_id_) {
        /* Child is ignored because it is in another thread. */
        remaining_new = remaining_new.drop_front(1);
        continue;
      }
    }
    while (true) {
      if (!new_vec.is_empty()) {
        ProfileNode *existing_child = new_vec.last();
        if (ProfileNode::time_overlap(*existing_child, *new_child)) {
          /* Node collides with previously added node. */
          remaining_new = remaining_new.drop_front(1);
          break;
        }
      }
      if (remaining_existing.is_empty()) {
        /* There are no remaining existing nodes the new child can collide with. */
        new_vec.append(new_child);
        tot_newly_inserted++;
        remaining_new[0] = nullptr;
        remaining_new = remaining_new.drop_front(1);
        break;
      }
      ProfileNode *existing_child = remaining_existing[0];
      if (existing_child->end_time_ <= new_child->begin_time_) {
        /* Existing child is completely before the new one. */
        new_vec.append(existing_child);
        remaining_existing = remaining_existing.drop_front(1);
        continue;
      }
      if (existing_child->begin_time_ < new_child->end_time_) {
        /* Existing child collides with the new child. */
        new_vec.append(existing_child);
        remaining_existing = remaining_existing.drop_front(1);
        remaining_new = remaining_new.drop_front(1);
        break;
      }
      if (new_child->end_time_ <= existing_child->begin_time_) {
        /* New child can be added safely. */
        new_vec.append(new_child);
        tot_newly_inserted++;
        remaining_new[0] = nullptr;
        remaining_new = remaining_new.drop_front(1);
        break;
      }
    }
  }
  new_vec.extend(remaining_existing);
  nodes_vec = std::move(new_vec);
  return tot_newly_inserted;
}

void ProfileLayout::add(const RecordedProfile &recorded_profile)
{
  /* Create new nodes for segments and add them to the id map. */
  for (const ProfileSegmentBegin &begin : recorded_profile.begins) {
    ProfileNode &node = *allocator_.construct<ProfileNode>().release();
    node.name_ = begin.name;
    node.begin_time_ = begin.time;
    node.end_time_ = TimePoint{}; /* The end time is not known yet. */
    node.id_ = begin.id;
    node.parent_id_ = begin.parent_id;
    node.thread_id_ = begin.thread_id;
    nodes_by_id_.add_new(begin.id, &node);
  }
  for (const ProfileSegmentEnd &end : recorded_profile.ends) {
    ProfileNode *node = nodes_by_id_.lookup_default(end.begin_id, nullptr);
    if (node != nullptr) {
      BLI_assert(node->end_time_ == TimePoint{});
      node->end_time_ = end.time;
    }
  }

  Set<ProfileNode *> nodes_with_new_children;

  /* Create parent/child relation ships for new nodes. */
  for (const ProfileSegmentBegin &begin : recorded_profile.begins) {
    ProfileNode *node = nodes_by_id_.lookup(begin.id);
    ProfileNode *parent_node = nodes_by_id_.lookup_default(begin.parent_id, nullptr);
    node->parent_ = parent_node;
    if (parent_node == nullptr) {
      if (root_thread_ids_.is_empty()) {
        begin_time_ = node->begin_time_;
        end_time_ = node->end_time_;
      }
      else {
        begin_time_ = std::min(begin_time_, node->begin_time_);
        end_time_ = std::max(end_time_, node->end_time_);
      }
      root_thread_ids_.append_non_duplicates(node->thread_id_);
      root_nodes_by_thread_id_.lookup_or_add_default(node->thread_id_).append(node);
    }
    else {
      parent_node->children_to_pack_.append(node);
      nodes_with_new_children.add(parent_node);
    }
  }

  /* Check if a previous root node is not a root anymore. */
  for (Vector<ProfileNode *> &nodes : root_nodes_by_thread_id_.values()) {
    Vector<ProfileNode *> nodes_that_are_not_root_anymore;
    for (ProfileNode *node : nodes) {
      ProfileNode *new_parent = nodes_by_id_.lookup_default(node->parent_id_, nullptr);
      if (new_parent != nullptr) {
        node->parent_ = new_parent;
        new_parent->children_to_pack_.append(node);
        nodes_with_new_children.add(new_parent);
        nodes_that_are_not_root_anymore.append_non_duplicates(node);
      }
    }
    for (ProfileNode *node : nodes_that_are_not_root_anymore) {
      nodes.remove_first_occurrence_and_reorder(node);
    }
  }

  /* Pack newly added children. */
  for (ProfileNode *node : nodes_with_new_children) {
    node->pack_added_children();
  }
}

void ProfileNode::destruct_recursively()
{
  for (ProfileNode *node : children_on_same_thread_) {
    node->destruct_recursively();
  }
  for (Span<ProfileNode *> nodes : packed_children_on_other_threads_) {
    for (ProfileNode *node : nodes) {
      node->destruct_recursively();
    }
  }
  this->~ProfileNode();
}

ProfileLayout::~ProfileLayout()
{
  for (Span<ProfileNode *> nodes : root_nodes_by_thread_id_.values()) {
    for (ProfileNode *node : nodes) {
      node->destruct_recursively();
    }
  }
}

}  // namespace blender::ed::info
