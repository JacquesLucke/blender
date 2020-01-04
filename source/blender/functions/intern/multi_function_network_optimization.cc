#include "FN_multi_function_network.h"
#include "FN_multi_function_network_optimization.h"
#include "FN_multi_functions.h"

#include "BLI_stack_cxx.h"

namespace FN {

using BLI::Stack;

static bool node_can_be_constant(MFBuilderNode &node)
{
  if (node.is_function()) {
    const MultiFunction &fn = node.as_function().function();
    if (fn.depends_on_context()) {
      return false;
    }

    /* TODO: Support vectors. */
    for (auto *socket : node.inputs()) {
      if (socket->data_type().is_vector()) {
        return false;
      }
    }
    for (auto *socket : node.outputs()) {
      if (socket->data_type().is_vector()) {
        return false;
      }
    }
    return true;
  }
  else {
    return false;
  }
}

void optimize_network__constant_folding(MFNetworkBuilder &network_builder,
                                        ResourceCollector &resources)
{
  Array<bool> is_constant(network_builder.nodes_by_id().size(), true);

  Stack<MFBuilderNode *> nodes_to_check = network_builder.nodes_by_id();

  while (!nodes_to_check.is_empty()) {
    MFBuilderNode &current_node = *nodes_to_check.pop();
    bool &current_node_is_constant = is_constant[current_node.id()];

    if (current_node_is_constant && !node_can_be_constant(current_node)) {
      current_node_is_constant = false;
    }

    if (!current_node_is_constant) {
      for (MFBuilderOutputSocket *output_socket : current_node.outputs()) {
        for (MFBuilderInputSocket *target_socket : output_socket->targets()) {
          MFBuilderNode &target_node = target_socket->node();
          bool &target_node_is_constant = is_constant[target_node.id()];
          if (target_node_is_constant) {
            target_node_is_constant = false;
            nodes_to_check.push(&target_node);
          }
        }
      }
    }
  }

  Set<MFBuilderNode *> constant_nodes;
  for (uint i : is_constant.index_range()) {
    if (is_constant[i]) {
      constant_nodes.add_new(network_builder.nodes_by_id()[i]);
    }
  }
  // network_builder.to_dot__clipboard(constant_nodes);

  Vector<MFBuilderOutputSocket *> builder_sockets_to_compute;
  Vector<uint> ids_to_compute;
  for (MFBuilderNode *node : constant_nodes) {
    if (node->inputs().size() == 0) {
      continue;
    }

    for (MFBuilderOutputSocket *output_socket : node->outputs()) {
      MFDataType data_type = output_socket->data_type();

      for (MFBuilderInputSocket *target_socket : output_socket->targets()) {
        if (!is_constant[target_socket->node().id()]) {

          MFBuilderDummyNode &dummy_node = network_builder.add_dummy(
              "Dummy", {data_type}, {}, {"Value"}, {});
          network_builder.add_link(*output_socket, dummy_node.input(0));
          ids_to_compute.append(dummy_node.input(0).id());
          builder_sockets_to_compute.append(output_socket);
          break;
        }
      }
    }
  }

  if (ids_to_compute.size() == 0) {
    return;
  }

  MFNetwork network{network_builder};

  Vector<const MFInputSocket *> sockets_to_compute;
  for (uint id : ids_to_compute) {
    sockets_to_compute.append(&network.socket_by_id(id).as_input());
  }

  MF_EvaluateNetwork network_function{{}, sockets_to_compute};

  MFContextBuilder context_builder;
  MFParamsBuilder params_builder{network_function, 1};

  for (uint param_index : network_function.param_indices()) {
    MFParamType param_type = network_function.param_type(param_index);
    BLI_assert(param_type.is_output());
    MFDataType data_type = param_type.data_type();

    switch (data_type.category()) {
      case MFDataType::Single: {
        const CPPType &cpp_type = data_type.single__cpp_type();
        void *buffer = resources.allocate(cpp_type.size(), cpp_type.alignment());
        GenericMutableArrayRef array{cpp_type, buffer, 1};
        params_builder.add_single_output(array);
        break;
      }
      case MFDataType::Vector: {
        BLI_assert(false);
        break;
      }
    }
  }

  network_function.call(IndexRange(1), params_builder, context_builder);

  for (uint param_index : network_function.param_indices()) {
    MFParamType param_type = network_function.param_type(param_index);
    MFDataType data_type = param_type.data_type();
    const CPPType &cpp_type = data_type.single__cpp_type();

    GenericMutableArrayRef array = params_builder.computed_array(param_index);
    void *buffer = array.buffer();
    resources.add(buffer, array.type().destruct_cb(), "Constant folded value");

    const MultiFunction &fn = resources.construct<MF_GenericConstantValue>(
        "Constant folded function", cpp_type, buffer);
    MFBuilderFunctionNode &folded_node = network_builder.add_function(fn);

    MFBuilderOutputSocket &original_socket = *builder_sockets_to_compute[param_index];
    Vector<MFBuilderInputSocket *> targets = original_socket.targets();

    for (MFBuilderInputSocket *target : targets) {
      network_builder.remove_link(original_socket, *target);
      network_builder.add_link(folded_node.output(0), *target);
    }
  }

  // network_builder.to_dot__clipboard();
}

}  // namespace FN
