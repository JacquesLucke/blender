/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_node_declaration.hh"

#include "BKE_node_runtime.hh"

#include "BLI_multi_value_map.hh"
#include "BLI_resource_scope.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"

namespace blender::bke::anonymous_attribute_inferencing {
namespace aal = nodes::aal;
using nodes::NodeDeclaration;

static const aal::RelationsInNode &get_relations_in_node(const bNode &node, ResourceScope &scope)
{
  if (const NodeDeclaration *node_decl = node.declaration()) {
    if (const aal::RelationsInNode *relations = node_decl->anonymous_attribute_relations()) {
      return *relations;
    }
  }
  return scope.construct<aal::RelationsInNode>();
}

static Vector<int> find_linked_group_inputs(
    const bNodeTree &tree,
    const bNodeSocket &group_output_socket,
    const FunctionRef<Vector<int>(const bNodeSocket &)> get_linked_inputs)
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
        if (!link->is_muted() && link->is_available()) {
          const bNodeSocket &from_socket = *link->fromsock;
          if (found_sockets.add(&from_socket)) {
            sockets_to_check.push(&from_socket);
          }
        }
      }
    }
    else {
      const bNode &node = socket.owner_node();
      for (const int input_index : get_linked_inputs(socket)) {
        const bNodeSocket &input_socket = node.input_socket(input_index);
        if (input_socket.is_available()) {
          if (found_sockets.add(&input_socket)) {
            sockets_to_check.push(&input_socket);
          }
        }
      }
    }
  }

  for (const bNode *node : tree.group_input_nodes()) {
    for (const bNodeSocket *socket : node->output_sockets()) {
      if (found_sockets.contains(socket)) {
        input_indices.append_non_duplicates(socket->index());
      }
    }
  }

  return input_indices;
}

bool update_anonymous_attribute_relations(const bNodeTree &tree)
{
  tree.ensure_topology_cache();

  ResourceScope scope;

  const Span<const bNode *> nodes = tree.all_nodes();
  Array<const aal::RelationsInNode *> relations_by_node(nodes.size());
  for (const int i : nodes.index_range()) {
    relations_by_node[i] = &get_relations_in_node(*nodes[i], scope);
  }

  std::unique_ptr<aal::RelationsInNode> new_relations = std::make_unique<aal::RelationsInNode>();
  const bNode *group_output_node = tree.group_output_node();
  if (!tree.has_available_link_cycle()) {
    if (group_output_node != nullptr) {
      for (const bNodeSocket *group_output_socket :
           group_output_node->input_sockets().drop_back(1)) {
        if (group_output_socket->type == SOCK_GEOMETRY) {
          const Vector<int> input_indices = find_linked_group_inputs(
              tree, *group_output_socket, [&](const bNodeSocket &output_socket) {
                Vector<int> indices;
                for (const aal::PropagateRelation &relation :
                     relations_by_node[output_socket.owner_node().index()]->propagate_relations) {
                  if (relation.to_geometry_output == output_socket.index()) {
                    indices.append(relation.from_geometry_input);
                  }
                }
                return indices;
              });
          for (const int input_index : input_indices) {
            aal::PropagateRelation relation;
            relation.from_geometry_input = input_index;
            relation.to_geometry_output = group_output_socket->index();
            BLI_assert(relation.from_geometry_input);
            BLI_assert(relation.to_geometry_output);
            new_relations->propagate_relations.append(relation);
          }
        }
        if (ELEM(group_output_socket->display_shape,
                 SOCK_DISPLAY_SHAPE_DIAMOND,
                 SOCK_DISPLAY_SHAPE_DIAMOND_DOT)) {
          const Vector<int> input_indices = find_linked_group_inputs(
              tree, *group_output_socket, [&](const bNodeSocket &output_socket) {
                Vector<int> indices;
                for (const aal::ReferenceRelation &relation :
                     relations_by_node[output_socket.owner_node().index()]->reference_relations) {
                  if (relation.to_field_output == output_socket.index()) {
                    indices.append(relation.from_field_input);
                  }
                }
                return indices;
              });
          for (const int input_index : input_indices) {
            aal::ReferenceRelation relation;
            relation.from_field_input = input_index;
            relation.to_field_output = group_output_socket->index();
            BLI_assert(relation.from_field_input);
            BLI_assert(relation.to_field_output);
            new_relations->reference_relations.append(relation);
          }
        }
      }
    }
  }

  std::cout << *new_relations << "\n";

  const bool group_interface_changed = !tree.runtime->anonymous_attribute_relations ||
                                       *tree.runtime->anonymous_attribute_relations !=
                                           *new_relations;
  tree.runtime->anonymous_attribute_relations = std::move(new_relations);

  return group_interface_changed;
}

}  // namespace blender::bke::anonymous_attribute_inferencing
