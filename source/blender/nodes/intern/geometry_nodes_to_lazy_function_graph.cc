/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_to_lazy_function_graph.hh"

#include "BLI_map.hh"

#include "BKE_geometry_set.hh"

namespace blender::modifiers::geometry_nodes {
using nodes::SocketRef;
const CPPType *get_socket_cpp_type(const SocketRef &socket);
}  // namespace blender::modifiers::geometry_nodes

namespace blender::nodes {

static const CPPType *get_vector_type(const CPPType &type)
{
  if (type.is<GeometrySet>()) {
    return &CPPType::get<Vector<GeometrySet>>();
  }
  return nullptr;
}

class GeometryNodeLazyFunction : public LazyFunction {
 public:
  GeometryNodeLazyFunction(const NodeRef &node,
                           Vector<const InputSocketRef *> &used_inputs,
                           Vector<const OutputSocketRef *> &used_outputs)
  {
    static_name_ = node.name().c_str();
    for (const InputSocketRef *socket : node.inputs()) {
      if (!socket->is_available()) {
        continue;
      }
      const CPPType *type = modifiers::geometry_nodes::get_socket_cpp_type(*socket);
      if (type == nullptr) {
        continue;
      }
      if (socket->is_multi_input_socket()) {
        type = get_vector_type(*type);
      }
      /* TODO: Name may not be static. */
      inputs_.append({socket->identifier().c_str(), *type});
      used_inputs.append(socket);
    }
    for (const OutputSocketRef *socket : node.outputs()) {
      if (!socket->is_available()) {
        continue;
      }
      const CPPType *type = modifiers::geometry_nodes::get_socket_cpp_type(*socket);
      if (type == nullptr) {
        continue;
      }
      outputs_.append({socket->identifier().c_str(), *type});
      used_outputs.append(socket);
    }
  }

  void execute_impl(fn::LazyFunctionParams &params) const override
  {
    UNUSED_VARS(params);
  }
};

class MultiInputLazyFunction : public LazyFunction {
 public:
  MultiInputLazyFunction(const InputSocketRef &socket)
  {
    static_name_ = "Multi Input";
    const CPPType *type = modifiers::geometry_nodes::get_socket_cpp_type(socket);
    BLI_assert(type != nullptr);
    BLI_assert(socket.is_multi_input_socket());
    for ([[maybe_unused]] const int i : socket.directly_linked_links().index_range()) {
      inputs_.append({"Input", *type});
    }
    const CPPType *vector_type = get_vector_type(*type);
    BLI_assert(vector_type != nullptr);
    outputs_.append({"Output", *vector_type});
  }

  void execute_impl(fn::LazyFunctionParams &params) const override
  {
    UNUSED_VARS(params);
  }
};

class ConversionLazyFunction : public LazyFunction {
 public:
  ConversionLazyFunction(const CPPType &from, const CPPType &to)
  {
    static_name_ = "Convert";
    inputs_.append({"From", from});
    outputs_.append({"To", to});
  }

  void execute_impl(fn::LazyFunctionParams &params) const override
  {
    UNUSED_VARS(params);
  }
};

void geometry_nodes_to_lazy_function_graph(const NodeTreeRef &tree,
                                           LazyFunctionGraph &graph,
                                           GeometryNodesLazyFunctionResources &resources)
{
  MultiValueMap<const InputSocketRef *, LFInputSocket *> input_socket_map;
  Map<const OutputSocketRef *, LFOutputSocket *> output_socket_map;
  Map<const InputSocketRef *, LFNode *> multi_input_socket_nodes;

  for (const NodeRef *node_ref : tree.nodes()) {
    if (node_ref->is_frame()) {
      continue;
    }
    Vector<const InputSocketRef *> used_inputs;
    Vector<const OutputSocketRef *> used_outputs;
    auto fn = std::make_unique<GeometryNodeLazyFunction>(*node_ref, used_inputs, used_outputs);
    LFNode &node = graph.add_node(*fn);
    resources.functions.append(std::move(fn));

    for (const int i : used_inputs.index_range()) {
      const InputSocketRef &socket_ref = *used_inputs[i];
      LFInputSocket &socket = node.input(i);

      if (socket_ref.is_multi_input_socket()) {
        auto fn = std::make_unique<MultiInputLazyFunction>(socket_ref);
        LFNode &multi_input_node = graph.add_node(*fn);
        resources.functions.append(std::move(fn));
        graph.add_link(multi_input_node.output(0), socket);
        multi_input_socket_nodes.add(&socket_ref, &multi_input_node);
      }
      else {
        input_socket_map.add(&socket_ref, &socket);
      }
    }
    for (const int i : used_outputs.index_range()) {
      output_socket_map.add_new(used_outputs[i], &node.output(i));
    }
  }

  for (const auto item : output_socket_map.items()) {
    const OutputSocketRef &from_ref = *item.key;
    LFOutputSocket &from = *item.value;
    const Span<const LinkRef *> links_from_socket = from_ref.directly_linked_links();
    const CPPType &from_type = *modifiers::geometry_nodes::get_socket_cpp_type(from_ref);

    struct TypeWithLinks {
      const CPPType *type;
      Vector<const LinkRef *> links;
    };

    Vector<TypeWithLinks> types_with_links;
    for (const LinkRef *link : links_from_socket) {
      const InputSocketRef &to_socket = link->to();
      if (!to_socket.is_available()) {
        continue;
      }
      const CPPType *to_type = modifiers::geometry_nodes::get_socket_cpp_type(to_socket);
      if (to_type == nullptr) {
        continue;
      }
      bool inserted = false;
      for (TypeWithLinks &type_with_links : types_with_links) {
        if (type_with_links.type == to_type) {
          type_with_links.links.append(link);
          inserted = true;
        }
      }
      if (inserted) {
        continue;
      }
      types_with_links.append({to_type, {link}});
    }

    for (const TypeWithLinks &type_with_links : types_with_links) {
      const CPPType &to_type = *type_with_links.type;
      const Span<const LinkRef *> links = type_with_links.links;
      LFOutputSocket *final_from_socket = nullptr;
      if (from_type == to_type) {
        final_from_socket = &from;
      }
      else {
        auto fn = std::make_unique<ConversionLazyFunction>(from_type, to_type);
        LFNode &conversion_node = graph.add_node(*fn);
        resources.functions.append(std::move(fn));
        graph.add_link(from, conversion_node.input(0));
        final_from_socket = &conversion_node.output(0);
      }

      for (const LinkRef *link_ref : links) {
        const InputSocketRef &to_socket_ref = link_ref->to();
        if (to_socket_ref.is_multi_input_socket()) {
          LFNode *multi_input_node = multi_input_socket_nodes.lookup_default(&to_socket_ref,
                                                                             nullptr);
          if (multi_input_node == nullptr) {
            continue;
          }
          /* TODO: Use stored link index, but need to validate it. */
          const int link_index = to_socket_ref.directly_linked_links().first_index_try(link_ref);
          graph.add_link(*final_from_socket, multi_input_node->input(link_index));
        }
        else {
          for (LFInputSocket *to : input_socket_map.lookup(&to_socket_ref)) {
            graph.add_link(*final_from_socket, *to);
          }
        }
      }
    }
  }
}

}  // namespace blender::nodes
