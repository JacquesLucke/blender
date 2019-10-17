#include "BKE_multi_function_network.h"

namespace BKE {
namespace MultiFunctionNetwork {

/* Network Builder
 **************************************/

NetworkBuilder::~NetworkBuilder()
{
  for (auto node : m_function_nodes) {
    delete node;
  }
  for (auto node : m_placeholder_nodes) {
    delete node;
  }
  for (auto socket : m_input_sockets) {
    delete socket;
  }
  for (auto socket : m_output_sockets) {
    delete socket;
  }
}

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

  auto node = new BuilderFunctionNode();

  node->m_network = this;
  node->m_is_placeholder = false;
  node->m_function = &function;
  node->m_input_param_indices = input_param_indices;
  node->m_output_param_indices = output_param_indices;
  node->m_id = m_node_by_id.size();

  for (uint i = 0; i < input_param_indices.size(); i++) {
    ParamType param = function.signature().param_types()[i];
    BLI_assert(param.is_input());

    auto input_socket = new BuilderInputSocket();
    input_socket->m_type = param.as_data_type();
    input_socket->m_node = node;
    input_socket->m_index = i;
    input_socket->m_is_output = false;
    input_socket->m_id = m_socket_by_id.size();
    node->m_inputs.append(input_socket);
    m_socket_by_id.append(input_socket);
    m_input_sockets.append(input_socket);
  }

  for (uint i = 0; i < output_param_indices.size(); i++) {
    ParamType param = function.signature().param_types()[i];
    BLI_assert(param.is_output());

    auto output_socket = new BuilderOutputSocket();
    output_socket->m_type = param.as_data_type();
    output_socket->m_node = node;
    output_socket->m_index = i;
    output_socket->m_is_output = true;
    output_socket->m_id = m_socket_by_id.size();
    node->m_outputs.append(output_socket);
    m_socket_by_id.append(output_socket);
    m_output_sockets.append(output_socket);
  }

  m_function_nodes.append(node);
  m_node_by_id.append(node);
  return *node;
}

BuilderPlaceholderNode &NetworkBuilder::add_placeholder(
    ArrayRef<MultiFunctionDataType> input_types, ArrayRef<MultiFunctionDataType> output_types)
{
  auto node = new BuilderPlaceholderNode();

  node->m_network = this;
  node->m_is_placeholder = true;
  node->m_id = m_node_by_id.size();

  for (uint i = 0; i < input_types.size(); i++) {
    auto input_socket = new BuilderInputSocket();
    input_socket->m_type = input_types[i];
    input_socket->m_node = node;
    input_socket->m_index = i;
    input_socket->m_is_output = false;
    input_socket->m_id = m_socket_by_id.size();
    node->m_inputs.append(input_socket);
    m_socket_by_id.append(input_socket);
    m_input_sockets.append(input_socket);
  }
  for (uint i = 0; i < output_types.size(); i++) {
    auto output_socket = new BuilderOutputSocket();
    output_socket->m_type = output_types[i];
    output_socket->m_node = node;
    output_socket->m_index = i;
    output_socket->m_is_output = true;
    output_socket->m_id = m_socket_by_id.size();
    node->m_outputs.append(output_socket);
    m_socket_by_id.append(output_socket);
    m_output_sockets.append(output_socket);
  }

  m_placeholder_nodes.append(node);
  m_node_by_id.append(node);
  return *node;
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
  m_node_by_id = Array<Node *>(builder->nodes_by_id().size());
  m_socket_by_id = Array<Socket *>(builder->sockets_by_id().size());

  for (BuilderFunctionNode *builder_node : builder->function_nodes()) {
    FunctionNode *node = new FunctionNode();

    node->m_function = &builder_node->function();
    node->m_id = builder_node->id();
    node->m_input_param_indices = builder_node->input_param_indices();
    node->m_output_param_indices = builder_node->output_param_indices();
    node->m_network = this;
    node->m_is_placeholder = false;

    for (BuilderInputSocket *builder_socket : builder_node->inputs()) {
      InputSocket *socket = new InputSocket();
      socket->m_id = builder_socket->id();
      socket->m_index = builder_socket->index();
      socket->m_is_output = false;
      socket->m_node = node;
      socket->m_type = builder_socket->type();

      m_socket_by_id[socket->id()] = socket;
      m_input_sockets.append(socket);
      node->m_inputs.append(socket);
    }
    for (BuilderOutputSocket *builder_socket : builder_node->outputs()) {
      OutputSocket *socket = new OutputSocket();
      socket->m_id = builder_socket->id();
      socket->m_index = builder_socket->index();
      socket->m_is_output = true;
      socket->m_node = node;
      socket->m_type = builder_socket->type();

      m_socket_by_id[socket->id()] = socket;
      m_output_sockets.append(socket);
      node->m_outputs.append(socket);
    }

    m_function_nodes.append(node);
    m_node_by_id[node->id()] = node;
  }

  for (BuilderPlaceholderNode *builder_node : builder->placeholder_nodes()) {
    PlaceholderNode *node = new PlaceholderNode();

    node->m_id = builder_node->id();
    node->m_network = this;
    node->m_is_placeholder = false;

    for (BuilderInputSocket *builder_socket : builder_node->inputs()) {
      InputSocket *socket = new InputSocket();
      socket->m_id = builder_socket->id();
      socket->m_index = builder_socket->index();
      socket->m_is_output = false;
      socket->m_node = node;
      socket->m_type = builder_socket->type();

      m_socket_by_id[socket->id()] = socket;
      m_input_sockets.append(socket);
      node->m_inputs.append(socket);
    }
    for (BuilderOutputSocket *builder_socket : builder_node->outputs()) {
      OutputSocket *socket = new OutputSocket();
      socket->m_id = builder_socket->id();
      socket->m_index = builder_socket->index();
      socket->m_is_output = true;
      socket->m_node = node;
      socket->m_type = builder_socket->type();

      m_socket_by_id[socket->id()] = socket;
      m_output_sockets.append(socket);
      node->m_outputs.append(socket);
    }

    m_placeholder_nodes.append(node);
    m_node_by_id[node->id()] = node;
  }

  for (BuilderInputSocket *builder_socket : builder->input_sockets()) {
    InputSocket &socket = m_socket_by_id[builder_socket->id()]->as_input();
    OutputSocket &origin = m_socket_by_id[builder_socket->origin()->id()]->as_output();

    socket.m_origin = &origin;
  }

  for (BuilderOutputSocket *builder_socket : builder->output_sockets()) {
    OutputSocket &socket = m_socket_by_id[builder_socket->id()]->as_output();

    for (BuilderInputSocket *builder_target : builder_socket->targets()) {
      InputSocket &target = m_socket_by_id[builder_target->id()]->as_input();
      socket.m_targets.append(&target);
    }
  }
}

Network::~Network()
{
  for (auto node : m_function_nodes) {
    delete node;
  }
  for (auto node : m_placeholder_nodes) {
    delete node;
  }
  for (auto socket : m_input_sockets) {
    delete socket;
  }
  for (auto socket : m_output_sockets) {
    delete socket;
  }
}

}  // namespace MultiFunctionNetwork
}  // namespace BKE
