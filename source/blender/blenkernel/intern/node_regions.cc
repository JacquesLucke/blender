/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node_regions.hh"
#include "BKE_node_runtime.hh"

#include "BLI_bit_vector.hh"
#include "BLI_dot_export.hh"
#include "BLI_stack.hh"
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
  MutableSpan<NTreeRegion> regions = result.regions;

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
    const int boundary_index = boundary_node_indices.index_of_try(node);

    for (const bNodeSocket *input_socket : node->input_sockets()) {
      if (input_socket->is_available()) {
        for (const bNodeLink *link : input_socket->directly_linked_links()) {
          const bNodeSocket *source_socket = link->fromsock;
          if (source_socket->is_available()) {
            const bNode &source_node = source_socket->owner_node();
            const NodeInfo &source_info = info_by_node[source_node.index_in_tree()];

            if (boundary_index >= 0) {
              const BoundaryNode &boundary_node = boundary_nodes[boundary_index];
              const int region_index = boundary_node.region_index;
              for (const int after_region_index : source_info.after) {
                if (after_region_index == region_index && boundary_node.is_input) {
                  const_cast<bNodeLink *>(link)->flag &= ~NODE_LINK_VALID;
                }
                else {
                  info.after.append_non_duplicates(after_region_index);
                }
              }
            }
            else {
              info.after.extend_non_duplicates(source_info.after);
            }
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

    if (boundary_index >= 0) {
      const BoundaryNode &boundary_node = boundary_nodes[boundary_index];
      const int region_index = boundary_node.region_index;
      const NTreeRegionBounds &bound = region_bounds[region_index];
      for (const bNode *other_node : bound.inputs) {
        NodeInfo &other_node_info = info_by_node[other_node->index_in_tree()];
        other_node_info.after.extend_non_duplicates(info.after);
        other_node_info.inside.extend_non_duplicates(info.inside);
      }
      for (const bNode *other_node : bound.outputs) {
        NodeInfo &other_node_info = info_by_node[other_node->index_in_tree()];
        other_node_info.after.extend_non_duplicates(info.after);
        other_node_info.inside.extend_non_duplicates(info.inside);
      }
      for (const int parent_region_index : info.inside) {
        NTreeRegion &parent_region = regions[parent_region_index];
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

  // std::cout << "\n";
  // for (const bNode *node : toposort_left_to_right) {
  //   const NodeInfo &info = info_by_node[node->index_in_tree()];
  //   std::cout << node->name << "\n";
  //   std::cout << "  Inside: ";
  //   for (const int region_index : info.inside) {
  //     std::cout << region_index << ", ";
  //   }
  //   std::cout << "\n  After: ";
  //   for (const int region_index : info.after) {
  //     std::cout << region_index << ", ";
  //   }
  //   std::cout << "\n";
  // }
  // std::cout << "\n";

  for (const int i_main_region : IndexRange(num_regions)) {
    Stack<int> stack;
    Array<bool> seen(num_regions, false);
    for (const int i_child_region : regions[i_main_region].children_regions) {
      seen[i_child_region] = true;
      stack.push(i_child_region);
    }
    while (!stack.is_empty()) {
      const int i_current_region = stack.pop();
      if (i_current_region == i_main_region) {
        regions[i_main_region].is_in_cycle = true;
        break;
      }
      for (const int i_child_region : regions[i_current_region].children_regions) {
        if (!seen[i_child_region]) {
          stack.push(i_child_region);
          seen[i_child_region] = true;
        }
      }
    }
  }

  for (const int x : IndexRange(num_regions)) {
    NTreeRegion &x_region = regions[x];
    if (x_region.is_in_cycle) {
      continue;
    }
    for (const int y : IndexRange(num_regions)) {
      NTreeRegion &y_region = regions[y];
      for (const int z : IndexRange(num_regions)) {
        if (x_region.children_regions.contains(z) && x_region.children_regions.contains(y) &&
            y_region.children_regions.contains(z)) {
          x_region.children_regions.remove_first_occurrence_and_reorder(z);
        }
      }
    }
  }

  // dot::DirectedGraph digraph;
  // Vector<dot::Node *> dot_nodes;
  // for (const int region_index : regions.index_range()) {
  //   const NTreeRegion &region = regions[region_index];
  //   dot_nodes.append(&digraph.new_node(std::to_string(region_index)));
  // }
  // for (const int region_index : regions.index_range()) {
  //   const NTreeRegion &region = regions[region_index];
  //   for (const int child_region_index : region.children_regions) {
  //     if (region_index != child_region_index) {
  //       digraph.new_edge(*dot_nodes[region_index], *dot_nodes[child_region_index]);
  //     }
  //   }
  // }
  // std::cout << "\n\n" << digraph.to_dot_string() << "\n\n";

  for (const bNode *node : ntree.all_nodes()) {
    const NodeInfo &info = info_by_node[node->index_in_tree()];
    for (const int region_index : info.inside) {
      regions[region_index].contained_nodes.append(node);
    }
    const int boundary_index = boundary_node_indices.index_of_try(node);
    if (boundary_index >= 0) {
      const BoundaryNode &boundary_node = boundary_nodes[boundary_index];
      if (!boundary_node.is_input) {
        regions[boundary_node.region_index].contained_nodes.append(node);
      }
    }
  }

  for (const int region_index : IndexRange(num_regions)) {
    NTreeRegion &region = regions[region_index];
    for (const int child_region_index : region.children_regions) {
      const NTreeRegion &child_region = regions[child_region_index];
      region.contained_nodes.extend_non_duplicates(child_region.contained_nodes);
    }
  }

  /* Rules to enforce:
   * - Well defined parent hierarchy.
   * - Group output outside is outside of all regions.
   * - "after cycles" must not exist
   */

  return result;
}

}  // namespace blender::bke
