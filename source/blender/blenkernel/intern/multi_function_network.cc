#include "BKE_multi_function_network.h"

namespace BKE {
namespace MultiFunctionNetwork {

/* Network Builder
 **************************************/

BuilderFunctionNode &NetworkBuilder::add_function(MultiFunction &function,
                                                  ArrayRef<uint> input_param_indices,
                                                  ArrayRef<uint> output_param_indices)
{
#ifdef DEBUG
  BLI_assert(!input_param_indices.has_duplicates__linear_search());
  BLI_assert(!output_param_indices.has_duplicates__linear_search());
  for (uint i = 0; i < function.signature().param_types().size(); i++) {
    BLI_assert(input_param_indices.contains(i) || output_param_indices.contains(i));
  }
#endif

  auto node = BLI::make_unique<BuilderFunctionNode>();

  node->m_network = this;
  node->m_is_placeholder = false;
  node->m_function = &function;
  node->m_input_param_indices = input_param_indices;
  node->m_output_param_indices = output_param_indices;
  node->m_id = m_function_nodes.size() + m_placeholder_nodes.size();

  for (uint i = 0; i < input_param_indices.size(); i++) {
    ParamType param = function.signature().param_types()[i];
    BLI_assert(param.is_input());

    auto input_socket = BLI::make_unique<BuilderInputSocket>();
    input_socket->m_type = param.as_data_type();
    input_socket->m_node = node.get();
    input_socket->m_index = i;
    input_socket->m_is_output = false;
    input_socket->m_id = m_input_sockets.size() + m_output_sockets.size();
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
    output_socket->m_id = m_input_sockets.size() + m_output_sockets.size();
    node->m_outputs.append(output_socket.get());
    m_output_sockets.append(std::move(output_socket));
  }

  BuilderFunctionNode &node_ref = *node;
  m_function_nodes.append(std::move(node));
  return node_ref;
}

BuilderPlaceholderNode &NetworkBuilder::add_placeholder(
    ArrayRef<MultiFunctionDataType> input_types, ArrayRef<MultiFunctionDataType> output_types)
{
  auto node = BLI::make_unique<BuilderPlaceholderNode>();

  node->m_network = this;
  node->m_is_placeholder = true;
  node->m_id = m_function_nodes.size() + m_placeholder_nodes.size();

  for (uint i = 0; i < input_types.size(); i++) {
    auto input_socket = BLI::make_unique<BuilderInputSocket>();
    input_socket->m_type = input_types[i];
    input_socket->m_node = node.get();
    input_socket->m_index = i;
    input_socket->m_is_output = false;
    input_socket->m_id = m_input_sockets.size() + m_output_sockets.size();
    node->m_inputs.append(input_socket.get());
    m_input_sockets.append(std::move(input_socket));
  }
  for (uint i = 0; i < output_types.size(); i++) {
    auto output_socket = BLI::make_unique<BuilderOutputSocket>();
    output_socket->m_type = output_types[i];
    output_socket->m_node = node.get();
    output_socket->m_index = i;
    output_socket->m_is_output = true;
    output_socket->m_id = m_input_sockets.size() + m_output_sockets.size();
    node->m_outputs.append(output_socket.get());
    m_output_sockets.append(std::move(output_socket));
  }

  BuilderPlaceholderNode &node_ref = *node;
  m_placeholder_nodes.append(std::move(node));
  return node_ref;
}

void NetworkBuilder::add_link(BuilderOutputSocket &from, BuilderInputSocket &to)
{
  BLI_assert(to.origin() == nullptr);
  BLI_assert(from.m_node->m_network == to.m_node->m_network);
  from.m_targets.append(&to);
  to.m_origin = &from;
}

/* Network
 ********************************************/

Network::Network(std::unique_ptr<NetworkBuilder> builder)
{
}

}  // namespace MultiFunctionNetwork
}  // namespace BKE
