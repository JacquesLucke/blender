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

namespace blender::ed::info {

using profile::ProfileSegmentBegin;
using profile::ProfileSegmentEnd;

bool ProfileNode::time_overlap(const ProfileNode &a, const ProfileNode &b)
{
  const bool begin_of_a_is_in_b = (a.begin_time_ > b.begin_time_ && a.begin_time_ < b.end_time_);
  const bool begin_of_b_is_in_a = (b.begin_time_ > a.begin_time_ && b.begin_time_ < a.end_time_);
  return begin_of_a_is_in_b || begin_of_b_is_in_a;
}

void ProfileNode::add_child(ProfileNode *new_child)
{
  if (new_child->thread_id_ == thread_id_) {
    children_on_same_thread_.append(new_child);
    return;
  }
  for (Vector<ProfileNode *> &children_vec : packed_children_on_other_threads_) {
    bool overlap_found = false;
    for (ProfileNode *child : children_vec) {
      if (ProfileNode::time_overlap(*child, *new_child)) {
        overlap_found = true;
        break;
      }
    }
    if (!overlap_found) {
      children_vec.append(new_child);
      return;
    }
  }
  packed_children_on_other_threads_.append({});
  packed_children_on_other_threads_.last().append(new_child);
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
      parent_node->add_child(node);
    }
  }
  /* Check if a previous root node is not a root anymore. */
  for (Vector<ProfileNode *> &nodes : root_nodes_by_thread_id_.values()) {
    Vector<ProfileNode *> nodes_to_remove;
    for (ProfileNode *node : nodes) {
      ProfileNode *new_parent = nodes_by_id_.lookup_default(node->parent_id_, nullptr);
      if (new_parent != nullptr) {
        node->parent_ = new_parent;
        new_parent->add_child(node);
        nodes_to_remove.append_non_duplicates(node);
      }
    }
    for (ProfileNode *node : nodes_to_remove) {
      nodes.remove_first_occurrence_and_reorder(node);
    }
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
