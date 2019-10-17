#include "BKE_multi_function_network.h"

namespace BKE {
namespace MultiFunctionNetwork {

/* BuilderNetwork
 **************************************/

BuilderFunctionNode &BuilderNetwork::add_function(MultiFunction &function,
                                                  ArrayRef<uint> input_param_indices,
                                                  ArrayRef<uint> output_param_indices)
{
  BLI_assert(!input_param_indices.has_duplicates__linear_search());
  BLI_assert(!output_param_indices.has_duplicates__linear_search());

  auto node = BLI::make_unique<BuilderFunctionNode>();

  node->m_network = this;
  node->m_is_placeholder = false;
  node->m_function = &function;
  node->m_input_param_indices = input_param_indices;
  node->m_output_param_indices = output_param_indices;

  for (uint i = 0; i < input_param_indices.size(); i++) {
    ParamType param = function.signature().param_types()[i];
    BLI_assert(param.is_input());

    auto input_socket = BLI::make_unique<BuilderInputSocket>();
    input_socket->m_type = param.as_data_type();
    input_socket->m_node = node.get();
    input_socket->m_index = i;
    input_socket->m_is_output = false;
    node->m_inputs.append(input_socket.get());
    m_input_sockets.append(std::move(input_socket));
  }

  for (uint i = 0; i < output_param_indices.size(); i++) {
    ParamType param = function.signature().param_types()[i];
    BLI_assert(param.is_output());

    auto output_socket = BLI::make_unique<BuilderOutputSocket>();
    output_socket->m_type = param.as_data_type();
    output_socket->m_node = node.get();
    output_socket->m_index = i;
    output_socket->m_is_output = true;
    node->m_outputs.append(output_socket.get());
    m_output_sockets.append(std::move(output_socket));
  }

  BuilderFunctionNode &node_ref = *node;
  m_function_nodes.append(std::move(node));
  return node_ref;
}

BuilderPlaceholderNode &BuilderNetwork::add_placeholder(
    ArrayRef<MultiFunctionDataType> input_types, ArrayRef<MultiFunctionDataType> output_types)
{
  auto node = BLI::make_unique<BuilderPlaceholderNode>();

  node->m_network = this;
  node->m_is_placeholder = true;

  for (uint i = 0; i < input_types.size(); i++) {
    auto input_socket = BLI::make_unique<BuilderInputSocket>();
    input_socket->m_type = input_types[i];
    input_socket->m_node = node.get();
    input_socket->m_index = i;
    input_socket->m_is_output = false;
    node->m_inputs.append(input_socket.get());
    m_input_sockets.append(std::move(input_socket));
  }
  for (uint i = 0; i < output_types.size(); i++) {
    auto output_socket = BLI::make_unique<BuilderOutputSocket>();
    output_socket->m_type = output_types[i];
    output_socket->m_node = node.get();
    output_socket->m_index = i;
    output_socket->m_is_output = true;
    node->m_outputs.append(output_socket.get());
    m_output_sockets.append(std::move(output_socket));
  }

  BuilderPlaceholderNode &node_ref = *node;
  m_placeholder_nodes.append(std::move(node));
  return node_ref;
}

void BuilderNetwork::add_link(BuilderOutputSocket &from, BuilderInputSocket &to)
{
  BLI_assert(to.origin() == nullptr);
  BLI_assert(from.m_node->m_network == to.m_node->m_network);
  from.m_targets.append(&to);
  to.m_origin = &from;
}

}  // namespace MultiFunctionNetwork
}  // namespace BKE
