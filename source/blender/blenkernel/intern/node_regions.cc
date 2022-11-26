/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node_regions.hh"
#include "BKE_node_runtime.hh"

#include "BLI_bit_vector.hh"
#include "BLI_vector_set.hh"

namespace blender::bke {

struct BoundaryNode {
  const bNode *node;
  int region_index;
  bool is_input;
};

NTreeRegionResult analyze_node_context_regions(const bNodeTree &ntree,
                                               const Span<NTreeRegionBounds> region_bounds)
{
  ntree.ensure_topology_cache();

  const int num_nodes = ntree.all_nodes().size();
  const int num_regions = region_bounds.size();

  NTreeRegionResult result;
  result.regions.reinitialize(num_regions);

  Vector<BoundaryNode> boundary_nodes;
  VectorSet<const bNode *> boundary_node_indices;
  for (const int region_index : IndexRange(num_regions)) {
    const NTreeRegionBounds &bound = region_bounds[region_index];
    for (const bNode *node : bound.inputs) {
      boundary_nodes.append({node, region_index, true});
      boundary_node_indices.add_new(node);
    }
    for (const bNode *node : bound.outputs) {
      boundary_nodes.append({node, region_index, false});
      boundary_node_indices.add_new(node);
    }
  }

  const Span<const bNode *> toposort_left_to_right = ntree.toposort_left_to_right();

  struct NodeInfo {
    Vector<int> inside;
    Vector<int> after;
  };

  Array<NodeInfo> info_by_node(num_nodes);

  for (const bNode *node : toposort_left_to_right) {
    NodeInfo &info = info_by_node[node->index_in_tree()];
    for (const bNodeSocket *input_socket : node->input_sockets()) {
      if (input_socket->is_available()) {
        for (const bNodeSocket *source_socket : input_socket->directly_linked_sockets()) {
          if (source_socket->is_available()) {
            const bNode &source_node = source_socket->owner_node();
            const NodeInfo &source_info = info_by_node[source_node.index_in_tree()];
            info.after.extend_non_duplicates(source_info.after);
          }
        }
      }
    }
    for (const bNodeSocket *input_socket : node->input_sockets()) {
      if (input_socket->is_available()) {
        for (const bNodeLink *link : input_socket->directly_linked_links()) {
          const bNodeSocket *source_socket = link->fromsock;
          if (source_socket->is_available()) {
            const bNode &source_node = source_socket->owner_node();
            const NodeInfo &source_info = info_by_node[source_node.index_in_tree()];
            for (const int region_index : source_info.inside) {
              if (info.after.contains(region_index) || node->is_group_output()) {
                const_cast<bNodeLink *>(link)->flag &= ~NODE_LINK_VALID;
              }
              else {
                info.inside.append_non_duplicates(region_index);
              }
            }
          }
        }
      }
    }
    const int boundary_index = boundary_node_indices.index_of_try(node);
    if (boundary_index >= 0) {
      const BoundaryNode &boundary_node = boundary_nodes[boundary_index];
      const int region_index = boundary_node.region_index;
      for (const int parent_region_index : info.inside) {
        NTreeRegion &parent_region = result.regions[parent_region_index];
        parent_region.children_regions.append_non_duplicates(region_index);
      }
      if (boundary_node.is_input) {
        info.inside.append_non_duplicates(region_index);
      }
      else {
        if (info.inside.contains(region_index)) {
          info.inside.remove_first_occurrence_and_reorder(region_index);
        }
        info.after.append_non_duplicates(region_index);
      }
    }
  }

  std::cout << "\n";
  for (const bNode *node : toposort_left_to_right) {
    const NodeInfo &info = info_by_node[node->index_in_tree()];
    std::cout << node->name << "\n";
    std::cout << "  Inside: ";
    for (const int region_index : info.inside) {
      std::cout << region_index << ", ";
    }
    std::cout << "\n  After: ";
    for (const int region_index : info.after) {
      std::cout << region_index << ", ";
    }
    std::cout << "\n";
  }
  std::cout << "\n";

  for (const bNode *node : ntree.all_nodes()) {
    const NodeInfo &info = info_by_node[node->index_in_tree()];
    for (const int region_index : info.inside) {
      result.regions[region_index].contained_nodes.append(node);
    }
    const int boundary_index = boundary_node_indices.index_of_try(node);
    if (boundary_index >= 0) {
      const BoundaryNode &boundary_node = boundary_nodes[boundary_index];
      if (!boundary_node.is_input) {
        result.regions[boundary_node.region_index].contained_nodes.append(node);
      }
    }
  }

  for (const int region_index : IndexRange(num_regions)) {
    NTreeRegion &region = result.regions[region_index];
    for (const int child_region_index : region.children_regions) {
      const NTreeRegion &child_region = result.regions[child_region_index];
      region.contained_nodes.extend_non_duplicates(child_region.contained_nodes);
    }
  }

  /* Rules to enforce:
   * - Well defined parent hierarchy.
   * - Group output outside is outside of all regions.
   */

  return result;
}

}  // namespace blender::bke
