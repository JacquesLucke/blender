/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_node_declaration.hh"

#include "BKE_node_runtime.hh"

#include "BLI_multi_value_map.hh"
#include "BLI_resource_scope.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"

namespace blender::bke::anonymous_attribute_inferencing {
namespace aal = nodes::aal;

static const aal::RelationsInNode &get_relations_in_node(const bNode &node, ResourceScope &scope)
{
  return scope.construct<aal::RelationsInNode>();
}

static Vector<int> get_propagated_geometry_inputs(const bNodeTree &tree,
                                                  const bNodeSocket &group_output_socket)
{
  Set<const bNodeSocket *> found_sockets;
  Stack<const bNodeSocket *> sockets_to_check;

  Vector<int> input_indices;

  found_sockets.add_new(&group_output_socket);
  sockets_to_check.push(&group_output_socket);

  while (!sockets_to_check.is_empty()) {
    const bNodeSocket &socket = *sockets_to_check.pop();
    if (socket.is_input()) {
      for (const bNodeLink *link : socket.directly_linked_links()) {
        if (link->is_muted()) {
          continue;
        }
        const bNodeSocket &from_socket = *link->fromsock;
        if (!from_socket.is_available()) {
          continue;
        }
        if (found_sockets.add(&from_socket)) {
          sockets_to_check.push(&from_socket);
        }
      }
    }
    else {
      const
    }
  }

  return input_indices;
}

static Vector<aal::PropagateAttributeRelation> get_propagation_relations(const bNodeTree &tree)
{
  Vector<aal::PropagateAttributeRelation> propagate_relations;
  const bNode *group_output_node = tree.group_output_node();
  if (group_output_node == nullptr) {
    return propagate_relations;
  }

  Set<const bNode *> found_nodes;
  Stack<const bNode *> nodes_to_check;

  found_nodes.add(group_output_node);
  nodes_to_check.push(group_output_node);

  MultiValueMap<const bNodeSocket *, int> propagated_to_outputs_map;

  while (!nodes_to_check.is_empty()) {
    const bNode *node = nodes_to_check.pop();
  }

  return propagate_relations;
}

bool update_anonymous_attribute_relations(const bNodeTree &tree)
{
  tree.ensure_topology_cache();

  ResourceScope scope;

  // Array<const aal::RelationsInNode *> relations_by_node(tree.all_nodes().size());

  std::unique_ptr<aal::RelationsInNode> new_relations = std::make_unique<aal::RelationsInNode>();
  if (!tree.has_available_link_cycle()) {
    new_relations->propagate_attribute_relations = get_propagation_relations(tree);
  }

  const bool group_interface_changed = !tree.runtime->anonymous_attribute_relations ||
                                       *tree.runtime->anonymous_attribute_relations !=
                                           *new_relations;
  tree.runtime->anonymous_attribute_relations = std::move(new_relations);

  return group_interface_changed;
}

}  // namespace blender::bke::anonymous_attribute_inferencing
