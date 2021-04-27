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

#include "profiler_layout.hh"

#include "BLI_set.hh"

namespace blender::ed::profiler {

using profile::ProfileTaskBegin;
using profile::ProfileTaskEnd;

bool ProfileNode::time_overlap(const ProfileNode &a, const ProfileNode &b)
{
  const bool begin_of_a_is_in_b = (a.begin_time_ > b.begin_time_ && a.begin_time_ < b.end_time_);
  const bool begin_of_b_is_in_a = (b.begin_time_ > a.begin_time_ && b.begin_time_ < a.end_time_);
  return begin_of_a_is_in_b || begin_of_b_is_in_a;
}

template<typename UseNodeF>
static bool try_pack_into_vector(Vector<ProfileNode *> &sorted_nodes_vec,
                                 MutableSpan<ProfileNode *> sorted_nodes_to_pack,
                                 const UseNodeF &use_node_fn)
{
  bool packed_everything = true;

  MutableSpan<ProfileNode *> remaining_existing = sorted_nodes_vec;
  MutableSpan<ProfileNode *> remaining_new = sorted_nodes_to_pack;
  Vector<ProfileNode *> new_vec;
  while (!remaining_new.is_empty()) {
    ProfileNode *new_child = remaining_new[0];
    if (new_child == nullptr) {
      /* Child has been inserted already. */
      remaining_new = remaining_new.drop_front(1);
      continue;
    }
    if (!use_node_fn(*new_child)) {
      remaining_new = remaining_new.drop_front(1);
      continue;
    }
    while (true) {
      if (!new_vec.is_empty()) {
        ProfileNode *existing_child = new_vec.last();
        if (ProfileNode::time_overlap(*existing_child, *new_child)) {
          /* Node collides with previously added node. */
          remaining_new = remaining_new.drop_front(1);
          packed_everything = false;
          break;
        }
      }
      if (remaining_existing.is_empty()) {
        /* There are no remaining existing nodes the new child can collide with. */
        new_vec.append(new_child);
        remaining_new[0] = nullptr;
        remaining_new = remaining_new.drop_front(1);
        break;
      }
      ProfileNode *existing_child = remaining_existing[0];
      if (existing_child->end_time() <= new_child->begin_time()) {
        /* Existing child is completely before the new one. */
        new_vec.append(existing_child);
        remaining_existing = remaining_existing.drop_front(1);
        continue;
      }
      if (existing_child->begin_time() < new_child->end_time()) {
        /* Existing child collides with the new child. */
        new_vec.append(existing_child);
        remaining_existing = remaining_existing.drop_front(1);
        remaining_new = remaining_new.drop_front(1);
        packed_everything = false;
        break;
      }
      if (new_child->end_time() <= existing_child->begin_time()) {
        /* New child can be added safely. */
        new_vec.append(new_child);
        remaining_new[0] = nullptr;
        remaining_new = remaining_new.drop_front(1);
        break;
      }
    }
  }
  new_vec.extend(remaining_existing);
  sorted_nodes_vec = std::move(new_vec);
  return packed_everything;
}

static void pack_into_vectors(Vector<Vector<ProfileNode *>> &sorted_node_vectors,
                              MutableSpan<ProfileNode *> sorted_nodes_to_pack)
{
  if (sorted_nodes_to_pack.is_empty()) {
    return;
  }

  int iteration = 0;
  while (true) {
    if (iteration == sorted_node_vectors.size()) {
      sorted_node_vectors.append({});
    }
    Vector<ProfileNode *> &children_vec = sorted_node_vectors[iteration];
    iteration++;
    const bool packed_all_nodes = try_pack_into_vector(
        children_vec, sorted_nodes_to_pack, [](ProfileNode &UNUSED(node)) { return true; });
    if (packed_all_nodes) {
      break;
    }
  }
}

static void sort_nodes_by_begin_time(MutableSpan<ProfileNode *> nodes)
{
  std::sort(nodes.begin(), nodes.end(), [](ProfileNode *a, ProfileNode *b) {
    return a->begin_time() < b->begin_time();
  });
}

void ProfileNode::pack_added_children()
{
  sort_nodes_by_begin_time(children_to_pack_);

  /* Assume already packed children are sorted by begin time. */
  try_pack_into_vector(
      direct_children_, children_to_pack_, [thread_id = this->thread_id_](ProfileNode &node) {
        return node.thread_id() == thread_id;
      });

  pack_into_vectors(parallel_children_, children_to_pack_);
  children_to_pack_.clear();
}

void ProfilerLayout::add(const RecordedProfile &recorded_profile)
{
  /* Create new nodes for segments and add them to the id map. */
  for (const ProfileTaskBegin &task_begin : recorded_profile.task_begins) {
    ProfileNode &node = *allocator_.construct<ProfileNode>().release();
    node.name_ = task_begin.name;
    node.begin_time_ = task_begin.time;
    node.end_time_ = TimePoint{}; /* The end time is not known yet. */
    node.id_ = task_begin.id;
    node.parent_id_ = task_begin.parent_id;
    node.thread_id_ = task_begin.thread_id;
    nodes_by_id_.add_new(task_begin.id, &node);
  }
  for (const ProfileTaskEnd &task_end : recorded_profile.task_ends) {
    ProfileNode *node = nodes_by_id_.lookup_default(task_end.begin_id, nullptr);
    if (node != nullptr) {
      BLI_assert(node->end_time_ == TimePoint{});
      node->end_time_ = task_end.time;
    }
  }

  Set<ProfileNode *> nodes_with_new_children;
  Vector<ProfileNode *> root_nodes_to_pack;

  /* Create parent/child relation ships for new nodes. */
  for (const ProfileTaskBegin &task_begin : recorded_profile.task_begins) {
    ProfileNode *node = nodes_by_id_.lookup(task_begin.id);
    ProfileNode *parent_node = nodes_by_id_.lookup_default(task_begin.parent_id, nullptr);
    node->parent_ = parent_node;
    if (parent_node == nullptr) {
      if (root_nodes_.is_empty()) {
        begin_time_ = node->begin_time_;
        end_time_ = node->end_time_;
      }
      else {
        begin_time_ = std::min(begin_time_, node->begin_time_);
        end_time_ = std::max(end_time_, node->end_time_);
      }
      root_nodes_to_pack.append(node);
    }
    else {
      parent_node->children_to_pack_.append(node);
      nodes_with_new_children.add(parent_node);
    }
  }

  /* Check if a previous root node is not a root anymore. */
  for (Vector<ProfileNode *> &nodes : root_nodes_) {
    Vector<ProfileNode *> nodes_that_are_not_root_anymore;
    for (ProfileNode *node : nodes) {
      ProfileNode *new_parent = nodes_by_id_.lookup_default(node->parent_id_, nullptr);
      if (new_parent != nullptr) {
        node->parent_ = new_parent;
        new_parent->children_to_pack_.append(node);
        nodes_with_new_children.add(new_parent);
        nodes_that_are_not_root_anymore.append(node);
      }
    }
    for (ProfileNode *node : nodes_that_are_not_root_anymore) {
      nodes.remove_first_occurrence_and_reorder(node);
    }
    if (!nodes_that_are_not_root_anymore.is_empty()) {
      sort_nodes_by_begin_time(nodes);
    }
  }

  /* Pack newly added children. */
  for (ProfileNode *node : nodes_with_new_children) {
    node->pack_added_children();
  }

  pack_into_vectors(root_nodes_, root_nodes_to_pack);
}

void ProfileNode::destruct_recursively()
{
  for (ProfileNode *node : direct_children_) {
    node->destruct_recursively();
  }
  for (Span<ProfileNode *> nodes : parallel_children_) {
    for (ProfileNode *node : nodes) {
      node->destruct_recursively();
    }
  }
  this->~ProfileNode();
}

ProfilerLayout::~ProfilerLayout()
{
  for (Span<ProfileNode *> nodes : root_nodes_) {
    for (ProfileNode *node : nodes) {
      node->destruct_recursively();
    }
  }
}

}  // namespace blender::ed::profiler
