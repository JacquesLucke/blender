#include "network.h"

namespace FN {

void MF_EvaluateNetwork::call(MFMask mask, MFParams params, MFContext context) const
{
  if (mask.indices_amount() == 0) {
    return;
  }

  Storage storage(mask);
  this->copy_inputs_to_storage(mask, params, storage);
  this->evaluate_network_to_compute_outputs(mask, context, storage);
  this->copy_computed_values_to_outputs(mask, params, storage);
}

BLI_NOINLINE void MF_EvaluateNetwork::copy_inputs_to_storage(MFMask mask,
                                                             MFParams params,
                                                             Storage &storage) const
{
  for (uint input_index : m_inputs.index_iterator()) {
    const MFOutputSocket &socket = *m_inputs[input_index];
    switch (socket.data_type().category()) {
      case MFDataType::Single: {
        GenericVirtualListRef input_list = params.readonly_single_input(input_index, "Input");
        for (const MFInputSocket *target : socket.targets()) {
          const MFNode &target_node = target->node();
          if (target_node.is_function()) {
            MFParamType param_type = target->param_type();

            if (param_type.is_single_input()) {
              storage.set_virtual_list_for_input(*target, input_list);
            }
            else if (param_type.is_mutable_single()) {
              GenericMutableArrayRef array = storage.allocate_array(
                  param_type.data_type().single__cpp_type());
              for (uint i : mask.indices()) {
                array.copy_in__uninitialized(i, input_list[i]);
              }
              storage.set_array_ref_for_input(*target, array);
            }
            else {
              BLI_assert(false);
            }
          }
          else {
            storage.set_virtual_list_for_input(*target, input_list);
          }
        }
        break;
      }
      case MFDataType::Vector: {
        GenericVirtualListListRef input_list_list = params.readonly_vector_input(input_index,
                                                                                 "Input");
        for (const MFInputSocket *target : socket.targets()) {
          const MFNode &target_node = target->node();
          if (target_node.is_function()) {
            MFParamType param_type = target->param_type();

            if (param_type.is_vector_input()) {
              storage.set_virtual_list_list_for_input(*target, input_list_list);
            }
            else if (param_type.is_mutable_vector()) {
              GenericVectorArray &vector_array = storage.allocate_vector_array(
                  param_type.data_type().vector__cpp_base_type());
              for (uint i : mask.indices()) {
                vector_array.extend_single__copy(i, input_list_list[i]);
              }
              storage.set_vector_array_for_input(*target, vector_array);
            }
            else {
              BLI_assert(false);
            }
          }
          else {
            storage.set_virtual_list_list_for_input(*target, input_list_list);
          }
        }
        break;
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::evaluate_network_to_compute_outputs(
    MFMask mask, MFContext &global_context, Storage &storage) const
{
  Stack<const MFSocket *> sockets_to_compute;

  for (const MFInputSocket *input_socket : m_outputs) {
    sockets_to_compute.push(input_socket);
  }

  while (!sockets_to_compute.empty()) {
    const MFSocket &socket = *sockets_to_compute.peek();

    if (socket.is_input()) {
      const MFInputSocket &input_socket = socket.as_input();
      if (storage.input_is_computed(input_socket)) {
        sockets_to_compute.pop();
      }
      else {
        const MFOutputSocket &origin = input_socket.origin();
        sockets_to_compute.push(&origin);
      }
    }
    else {
      const MFOutputSocket &output_socket = socket.as_output();
      const MFFunctionNode &function_node = output_socket.node().as_function();

      uint not_computed_inputs_amount = 0;
      for (const MFInputSocket *input_socket : function_node.inputs()) {
        if (!storage.input_is_computed(*input_socket)) {
          not_computed_inputs_amount++;
          sockets_to_compute.push(input_socket);
        }
      }

      bool all_inputs_are_computed = not_computed_inputs_amount == 0;
      if (all_inputs_are_computed) {
        this->compute_and_forward_outputs(mask, global_context, function_node, storage);
        sockets_to_compute.pop();
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::compute_and_forward_outputs(
    MFMask mask,
    MFContext &global_context,
    const MFFunctionNode &function_node,
    Storage &storage) const
{
  const MultiFunction &function = function_node.function();
  MFParamsBuilder params_builder(function, mask.min_array_size());

  for (uint param_index : function.param_indices()) {
    MFParamType param_type = function.param_type(param_index);
    switch (param_type.type()) {
      case MFParamType::SingleInput: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericVirtualListRef values = storage.get_virtual_list_for_input(input_socket);
        params_builder.add_readonly_single_input(values);
        break;
      }
      case MFParamType::VectorInput: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericVirtualListListRef values = storage.get_virtual_list_list_for_input(input_socket);
        params_builder.add_readonly_vector_input(values);
        break;
      }
      case MFParamType::SingleOutput: {
        const MFOutputSocket &output_socket = function_node.output_for_param(param_index);
        GenericMutableArrayRef values_destination = storage.allocate_array(
            output_socket.data_type().single__cpp_type());
        params_builder.add_single_output(values_destination);
        break;
      }
      case MFParamType::VectorOutput: {
        const MFOutputSocket &output_socket = function_node.output_for_param(param_index);
        GenericVectorArray &values_destination = storage.allocate_vector_array(
            output_socket.data_type().vector__cpp_base_type());
        params_builder.add_vector_output(values_destination);
        break;
      }
      case MFParamType::MutableVector: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericVectorArray &values = storage.get_vector_array_for_input(input_socket);
        params_builder.add_mutable_vector(values);
        break;
      }
      case MFParamType::MutableSingle: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericMutableArrayRef values = storage.get_array_ref_for_input(input_socket);
        params_builder.add_mutable_single(values);
        break;
      }
    }
  }

  function.call(mask, params_builder, global_context);

  for (uint param_index : function.param_indices()) {
    MFParamType param_type = function.param_type(param_index);

    switch (param_type.type()) {
      case MFParamType::SingleInput:
      case MFParamType::VectorInput:
        break;
      case MFParamType::SingleOutput:
      case MFParamType::MutableSingle: {
        const MFOutputSocket &output_socket = function_node.output_for_param(param_index);
        GenericMutableArrayRef computed_values = params_builder.computed_array(param_index);
        for (const MFInputSocket *target : output_socket.targets()) {
          if (target->node().is_dummy()) {
            if (m_outputs.contains(target)) {
              storage.set_virtual_list_for_input(*target, computed_values);
            }
          }
          else {
            MFParamType target_param_type = target->param_type();
            if (target_param_type.is_single_input()) {
              storage.set_virtual_list_for_input(*target, computed_values);
            }
            else if (target_param_type.is_mutable_single()) {
              const CPPType &type = param_type.data_type().single__cpp_type();
              GenericMutableArrayRef copied_values = storage.allocate_array(type);
              for (uint i : mask.indices()) {
                type.copy_to_uninitialized(computed_values[i], copied_values[i]);
              }
              storage.set_array_ref_for_input(*target, copied_values);
            }
            else {
              BLI_assert(false);
            }
          }
        }
        break;
      }
      case MFParamType::VectorOutput:
      case MFParamType::MutableVector: {
        const MFOutputSocket &output_socket = function_node.output_for_param(param_index);
        GenericVectorArray &computed_values = params_builder.computed_vector_array(param_index);
        for (const MFInputSocket *target : output_socket.targets()) {
          if (target->node().is_dummy()) {
            if (m_outputs.contains(target)) {
              storage.set_virtual_list_list_for_input(*target, computed_values);
            }
          }
          else {
            MFParamType target_param_type = target->param_type();
            if (target_param_type.is_vector_input()) {
              storage.set_vector_array_for_input(*target, computed_values);
            }
            else if (target_param_type.is_mutable_vector()) {
              const CPPType &type = param_type.data_type().vector__cpp_base_type();
              GenericVectorArray &copied_values = storage.allocate_vector_array(type);
              for (uint i : mask.indices()) {
                copied_values.extend_single__copy(i, computed_values[i]);
              }
              storage.set_vector_array_for_input(*target, copied_values);
            }
            else {
              BLI_assert(false);
            }
          }
        }
        break;
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::copy_computed_values_to_outputs(MFMask mask,
                                                                      MFParams params,
                                                                      Storage &storage) const
{
  for (uint output_index : m_outputs.index_iterator()) {
    uint global_param_index = m_inputs.size() + output_index;
    const MFInputSocket &socket = *m_outputs[output_index];
    switch (socket.data_type().category()) {
      case MFDataType::Single: {
        GenericVirtualListRef values = storage.get_virtual_list_for_input(socket);
        GenericMutableArrayRef output_values = params.uninitialized_single_output(
            global_param_index, "Output");
        for (uint i : mask.indices()) {
          output_values.copy_in__uninitialized(i, values[i]);
        }
        break;
      }
      case MFDataType::Vector: {
        GenericVirtualListListRef values = storage.get_virtual_list_list_for_input(socket);
        GenericVectorArray &output_values = params.vector_output(global_param_index, "Output");
        for (uint i : mask.indices()) {
          output_values.extend_single__copy(i, values[i]);
        }
        break;
      }
    }
  }
}

}  // namespace FN
