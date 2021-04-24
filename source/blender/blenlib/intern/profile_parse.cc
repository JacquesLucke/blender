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

#include "BLI_map.hh"
#include "BLI_profile_parse.hh"

namespace blender::profile {

struct SegmentNodePair {
  const ProfileSegment *segment;
  ProfileNode *node;
};

void ProfileResult::add(const Span<ProfileSegment> segments)
{
  for (const ProfileSegment &segment : segments) {
    ProfileNode &node = *allocator_.construct<ProfileNode>().release();
    node.begin_time_ = segment.begin_time;
    node.end_time_ = segment.end_time;
    node.name_ = segment.name;
    node.thread_id_ = segment.thread_id;
    nodes_by_id_.add_new(segment.id, &node);
  }
  for (const ProfileSegment &segment : segments) {
    ProfileNode *node = nodes_by_id_.lookup(segment.id);
    ProfileNode *parent_node = nodes_by_id_.lookup_default(segment.parent_id, nullptr);
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
      root_nodes_.append(node);
    }
    else {
      parent_node->children_.append(node);
    }
  }
}

void ProfileNode::destruct_recursively()
{
  for (ProfileNode *child : children_) {
    child->destruct_recursively();
  }
  this->~ProfileNode();
}

ProfileResult::~ProfileResult()
{
  for (ProfileNode *node : root_nodes_) {
    node->destruct_recursively();
  }
}

}  // namespace blender::profile
