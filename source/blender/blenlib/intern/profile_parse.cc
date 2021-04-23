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

ProfileResult::ProfileResult(const Span<ProfileSegment> segments)
{
  Map<uint64_t, SegmentNodePair> data_by_id;
  for (const ProfileSegment &segment : segments) {
    ProfileNode &node = *allocator_.construct<ProfileNode>().release();
    node.begin_time_ = segment.begin_time;
    node.end_time_ = segment.end_time;
    node.name_ = segment.name;
    node.thread_id_ = segment.thread_id;
    data_by_id.add_new(segment.id, {&segment, &node});
  }
  for (const SegmentNodePair &data : data_by_id.values()) {
    const uint64_t parent_id = data.segment->parent_id;
    SegmentNodePair *parent_data = data_by_id.lookup_ptr(parent_id);
    if (parent_data != nullptr) {
      /* Is this guarenteed when clocks are used in different threads? */
      BLI_assert(parent_data->node->begin_time_ <= data.node->begin_time_);
      BLI_assert(parent_data->node->end_time_ >= data.node->end_time_);
      parent_data->node->children_.append(data.node);
      data.node->parent_ = parent_data->node;
    }
  }
  for (const SegmentNodePair &data : data_by_id.values()) {
    if (data.node->parent_ == nullptr) {
      root_nodes_.append(data.node);
    }
  }
}

}  // namespace blender::profile
