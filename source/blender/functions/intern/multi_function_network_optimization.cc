#include "FN_multi_function_network.h"
#include "FN_multi_function_network_optimization.h"
#include "FN_multi_functions.h"

#include "BLI_stack_cxx.h"

namespace FN {

using BLI::Stack;

void optimize_network__constant_folding(MFNetworkBuilder &network_builder,
                                        ResourceCollector &resources)
{
  Array<bool> function_node_is_constant(network_builder.function_nodes().size(), true);

  Stack<MFBuilderNode *> nodes_to_check;
  nodes_to_check.push_multiple(network_builder.dummy_nodes().cast<MFBuilderNode *>());
  nodes_to_check.push_multiple(network_builder.function_nodes().cast<MFBuilderNode *>());

  while (!nodes_to_check.is_empty()) {
    MFBuilderNode &current_node = *nodes_to_check.pop();

    bool is_const = true;
    if (current_node.is_dummy()) {
      is_const = false;
    }
    else {
      MFBuilderFunctionNode &function_node = current_node.as_function();
      uint function_node_index = network_builder.current_index_of(function_node);
      if (!function_node_is_constant[function_node_index]) {
        is_const = false;
      }
      else {
        const MultiFunction &fn = function_node.function();
        if (fn.depends_on_context()) {
          is_const = false;
          function_node_is_constant[function_node_index] = false;
        }
      }
    }

    if (!is_const) {
      current_node.foreach_target_socket([&](MFBuilderInputSocket &target_socket) {
        MFBuilderNode &target_node = target_socket.node();
        if (target_node.is_function()) {
          bool &target_is_const = function_node_is_constant[network_builder.current_index_of(
              target_node.as_function())];
          if (target_is_const) {
            target_is_const = false;
            nodes_to_check.push(&target_node);
          }
        }
      });
    }
  }

  Set<MFBuilderFunctionNode *> constant_nodes;
  for (uint i : function_node_is_constant.index_range()) {
    if (function_node_is_constant[i]) {
      constant_nodes.add_new(network_builder.function_nodes()[i]);
    }
  }
  // network_builder.to_dot__clipboard(*(Set<MFBuilderNode *> *)&constant_nodes);

  Vector<MFBuilderOutputSocket *> builder_sockets_to_compute;
  Vector<MFBuilderDummyNode *> dummy_nodes_to_compute;
  for (MFBuilderNode *node : constant_nodes) {
    if (node->inputs().size() == 0) {
      continue;
    }

    for (MFBuilderOutputSocket *output_socket : node->outputs()) {
      MFDataType data_type = output_socket->data_type();

      for (MFBuilderInputSocket *target_socket : output_socket->targets()) {
        MFBuilderNode &target_node = target_socket->node();
        if (target_node.is_dummy()) {
          continue;
        }

        MFBuilderFunctionNode &target_function_node = target_node.as_function();
        uint target_node_index = network_builder.current_index_of(target_function_node);

        if (!function_node_is_constant[target_node_index]) {
          MFBuilderDummyNode &dummy_node = network_builder.add_dummy(
              "Dummy", {data_type}, {}, {"Value"}, {});
          network_builder.add_link(*output_socket, dummy_node.input(0));
          dummy_nodes_to_compute.append(&dummy_node);
          builder_sockets_to_compute.append(output_socket);
          break;
        }
      }
    }
  }

  if (dummy_nodes_to_compute.size() == 0) {
    return;
  }

  MFNetwork network{network_builder};

  Vector<const MFInputSocket *> sockets_to_compute;
  for (MFBuilderDummyNode *dummy_node : dummy_nodes_to_compute) {
    uint node_index = network_builder.current_index_of(*dummy_node);
    sockets_to_compute.append(&network.dummy_nodes()[node_index]->input(0));
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
        const CPPType &cpp_base_type = data_type.vector__cpp_base_type();
        GenericVectorArray &vector_array = resources.construct<GenericVectorArray>(
            "constant vector", cpp_base_type, 1);
        params_builder.add_vector_output(vector_array);
        break;
      }
    }
  }

  network_function.call(IndexRange(1), params_builder, context_builder);

  for (uint param_index : network_function.param_indices()) {
    MFParamType param_type = network_function.param_type(param_index);
    MFDataType data_type = param_type.data_type();

    const MultiFunction *constant_fn = nullptr;

    switch (data_type.category()) {
      case MFDataType::Single: {
        const CPPType &cpp_type = data_type.single__cpp_type();

        GenericMutableArrayRef array = params_builder.computed_array(param_index);
        void *buffer = array.buffer();
        resources.add(buffer, array.type().destruct_cb(), "Constant folded value");

        constant_fn = &resources.construct<MF_GenericConstantValue>(
            "Constant folded function", cpp_type, buffer);
        break;
      }
      case MFDataType::Vector: {
        GenericVectorArray &vector_array = params_builder.computed_vector_array(param_index);
        GenericArrayRef array = vector_array[0];
        constant_fn = &resources.construct<MF_GenericConstantVector>("Constant folded function",
                                                                     array);
        break;
      }
    }

    MFBuilderFunctionNode &folded_node = network_builder.add_function(*constant_fn);

    MFBuilderOutputSocket &original_socket = *builder_sockets_to_compute[param_index];
    Vector<MFBuilderInputSocket *> targets = original_socket.targets();

    for (MFBuilderInputSocket *target : targets) {
      network_builder.remove_link(original_socket, *target);
      network_builder.add_link(folded_node.output(0), *target);
    }
  }

  network_builder.to_dot__clipboard();
}

}  // namespace FN
