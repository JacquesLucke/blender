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

/** \file
 * \ingroup fn
 */

#include "FN_multi_function_network_optimization.hh"

#include "BLI_stack.hh"

namespace blender::fn {

static bool set_tag_and_check_if_modified(bool &tag, bool new_value)
{
  if (tag != new_value) {
    tag = new_value;
    return true;
  }
  else {
    return false;
  }
}

static Array<bool> find_nodes_to_the_left_of__inclusive__mask(MFNetwork &network,
                                                              Span<MFNode *> nodes)
{
  Array<bool> is_to_the_left(network.node_id_amount(), false);

  for (MFNode *node : nodes) {
    is_to_the_left[node->id()] = true;
  }

  Stack<MFNode *> nodes_to_check = nodes;
  while (!nodes_to_check.is_empty()) {
    MFNode &node = *nodes_to_check.pop();
    if (is_to_the_left[node.id()]) {
      node.foreach_origin_node([&](MFNode &other_node) {
        if (set_tag_and_check_if_modified(is_to_the_left[other_node.id()], true)) {
          nodes_to_check.push(&other_node);
        }
      });
    }
  }

  return is_to_the_left;
}

static void invert_bool_array(MutableSpan<bool> array)
{
  for (bool &value : array) {
    value = !value;
  }
}

static Vector<MFNode *> find_valid_nodes_by_mask(MFNetwork &network, Span<bool> id_mask)
{
  Vector<MFNode *> nodes;
  for (uint id : id_mask.index_range()) {
    if (id_mask[id]) {
      MFNode *node = network.node_or_null_by_id(id);
      if (node != nullptr) {
        nodes.append(node);
      }
    }
  }
  return nodes;
}

static Vector<MFNode *> find_nodes_not_to_the_left_of__exclusive(MFNetwork &network,
                                                                 Span<MFNode *> nodes)
{
  Array<bool> masked_nodes = find_nodes_to_the_left_of__inclusive__mask(network, nodes);
  invert_bool_array(masked_nodes);
  Vector<MFNode *> result = find_valid_nodes_by_mask(network, masked_nodes);
  return result;
}

void optimize_network__remove_unused_nodes(MFNetwork &network)
{
  Span<MFNode *> dummy_nodes = network.dummy_nodes();
  Vector<MFNode *> nodes_to_remove = find_nodes_not_to_the_left_of__exclusive(network,
                                                                              dummy_nodes);
  network.remove(nodes_to_remove);
}

}  // namespace blender::fn
