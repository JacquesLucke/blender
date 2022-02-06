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

#pragma once

/** \file
 * \ingroup fn
 */

#include "FN_sgraph.hh"

#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"

namespace blender::fn::sgraph {

template<typename SGraphAdapter> inline bool sgraph_adapter_is_valid(const SGraphAdapter &adapter)
{
  using NodeID = typename SGraphAdapter::NodeID;

  using InputSocketID = std::pair<NodeID, int>;
  using OutputSocketID = std::pair<NodeID, int>;

  Set<std::string> errors;

  Set<NodeID> all_nodes;
  adapter.foreach_node([&](const NodeID &node) {
    if (all_nodes.contains(node)) {
      errors.add("`foreach_node` iterates over same node more than once");
    }
    all_nodes.add(node);
  });

  MultiValueMap<InputSocketID, OutputSocketID> in_to_out_links;
  MultiValueMap<OutputSocketID, InputSocketID> out_to_in_links;

  for (const NodeID &node : all_nodes) {
    for (const int input_index : IndexRange(adapter.node_inputs_size(node))) {
      adapter.foreach_linked_output(
          node, input_index, [&](const NodeID &other_node, const int other_index) {
            if (node == other_node) {
              errors.add("link connects sockets of the same node");
            }
            if (!all_nodes.contains(other_node)) {
              errors.add("link connects to non-existant node");
            }
            if (other_index < 0) {
              errors.add("socket index is negative");
            }
            if (other_index >= adapter.node_outputs_size(other_node)) {
              errors.add("index is out of range");
            }
            in_to_out_links.add({node, input_index}, {other_node, other_index});
          });
    }
  }

  for (const NodeID &node : all_nodes) {
    for (const int output_index : IndexRange(adapter.node_outputs_size(node))) {
      adapter.foreach_linked_input(
          node, output_index, [&](const NodeID &other_node, const int other_index) {
            if (node == other_node) {
              errors.add("link connects sockets of the same node");
            }
            if (!all_nodes.contains(other_node)) {
              errors.add("link connects to non-existant node");
            }
            if (other_index < 0) {
              errors.add("socket index is negative");
            }
            if (other_index >= adapter.node_inputs_size(other_node)) {
              errors.add("index is out of range");
            }
            out_to_in_links.add({node, output_index}, {other_node, other_index});
          });
    }
  }

  for (const InputSocketID &input : in_to_out_links.keys()) {
    const Span<OutputSocketID> linked_outputs = in_to_out_links.lookup(input);
    for (const OutputSocketID &output : linked_outputs) {
      const Span<InputSocketID> linked_inputs = out_to_in_links.lookup(output);
      if (!linked_inputs.contains(input)) {
        errors.add("link iterators are inconsistent");
      }
    }
  }

  for (const OutputSocketID &output : out_to_in_links.keys()) {
    const Span<InputSocketID> linked_inputs = out_to_in_links.lookup(output);
    for (const InputSocketID &input : linked_inputs) {
      const Span<OutputSocketID> linked_outputs = in_to_out_links.lookup(input);
      if (!linked_outputs.contains(output)) {
        errors.add("link iterators are inconsistent");
      }
    }
  }

  for (const std::string &error : errors) {
    std::cerr << error << "\n";
  }

  return errors.is_empty();
}

}  // namespace blender::fn::sgraph
