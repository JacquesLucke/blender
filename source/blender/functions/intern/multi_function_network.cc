#include <sstream>

#include "FN_multi_function_network.h"

#include "BLI_set.h"
#include "BLI_stack_cxx.h"
#include "BLI_dot_export.h"

extern "C" {
void WM_clipboard_text_set(const char *buf, bool selection);
}

namespace FN {

using BLI::Map;
using BLI::Set;
using BLI::Stack;

/* MFNetwork Builder
 **************************************/

MFNetworkBuilder::~MFNetworkBuilder()
{
  for (auto node : m_function_nodes) {
    node->~MFBuilderFunctionNode();
  }
  for (auto node : m_dummy_nodes) {
    node->~MFBuilderDummyNode();
  }
  for (auto socket : m_input_sockets) {
    socket->~MFBuilderInputSocket();
  }
  for (auto socket : m_output_sockets) {
    socket->~MFBuilderOutputSocket();
  }
}

MFBuilderFunctionNode &MFNetworkBuilder::add_function(const MultiFunction &function)
{
  Vector<uint> input_param_indices;
  Vector<uint> output_param_indices;
  for (uint param_index : function.param_indices()) {
    switch (function.param_type(param_index).interface_type()) {
      case MFParamType::InterfaceType::Input: {
        input_param_indices.append(param_index);
        break;
      }
      case MFParamType::InterfaceType::Output: {
        output_param_indices.append(param_index);
        break;
      }
      case MFParamType::InterfaceType::Mutable: {
        input_param_indices.append(param_index);
        output_param_indices.append(param_index);
        break;
      }
    }
  }

  auto &node = *m_allocator.construct<MFBuilderFunctionNode>().release();

  node.m_network = this;
  node.m_is_dummy = false;
  node.m_function = &function;
  node.m_input_param_indices = input_param_indices;
  node.m_output_param_indices = output_param_indices;
  node.m_id = m_node_by_id.size();

  for (uint i : input_param_indices.index_iterator()) {
    uint param_index = input_param_indices[i];
    MFParamType param = function.param_type(param_index);
    BLI_assert(param.is_input_or_mutable());

    auto &input_socket = *m_allocator.construct<MFBuilderInputSocket>().release();
    input_socket.m_data_type = param.data_type();
    input_socket.m_node = &node;
    input_socket.m_index = i;
    input_socket.m_is_output = false;
    input_socket.m_id = m_socket_by_id.size();
    node.m_inputs.append(&input_socket);
    m_socket_by_id.append(&input_socket);
    m_input_sockets.append(&input_socket);
  }

  for (uint i : output_param_indices.index_iterator()) {
    uint param_index = output_param_indices[i];
    MFParamType param = function.param_type(param_index);
    BLI_assert(param.is_output_or_mutable());

    auto &output_socket = *m_allocator.construct<MFBuilderOutputSocket>().release();
    output_socket.m_data_type = param.data_type();
    output_socket.m_node = &node;
    output_socket.m_index = i;
    output_socket.m_is_output = true;
    output_socket.m_id = m_socket_by_id.size();
    node.m_outputs.append(&output_socket);
    m_socket_by_id.append(&output_socket);
    m_output_sockets.append(&output_socket);
  }

  m_function_nodes.append(&node);
  m_node_by_id.append(&node);
  return node;
}

MFBuilderDummyNode &MFNetworkBuilder::add_dummy(StringRef name,
                                                ArrayRef<MFDataType> input_types,
                                                ArrayRef<MFDataType> output_types,
                                                ArrayRef<StringRef> input_names,
                                                ArrayRef<StringRef> output_names)
{
  auto &node = *m_allocator.construct<MFBuilderDummyNode>().release();

  node.m_network = this;
  node.m_is_dummy = true;
  node.m_id = m_node_by_id.size();
  node.m_name = m_allocator.copy_string(name);

  for (uint i : input_types.index_iterator()) {
    auto &input_socket = *m_allocator.construct<MFBuilderInputSocket>().release();
    input_socket.m_data_type = input_types[i];
    input_socket.m_node = &node;
    input_socket.m_index = i;
    input_socket.m_is_output = false;
    input_socket.m_id = m_socket_by_id.size();
    node.m_inputs.append(&input_socket);
    node.m_input_names.append(m_allocator.copy_string(input_names[i]));
    m_socket_by_id.append(&input_socket);
    m_input_sockets.append(&input_socket);
  }
  for (uint i : output_types.index_iterator()) {
    auto &output_socket = *m_allocator.construct<MFBuilderOutputSocket>().release();
    output_socket.m_data_type = output_types[i];
    output_socket.m_node = &node;
    output_socket.m_index = i;
    output_socket.m_is_output = true;
    output_socket.m_id = m_socket_by_id.size();
    node.m_outputs.append(&output_socket);
    node.m_output_names.append(m_allocator.copy_string(output_names[i]));
    m_socket_by_id.append(&output_socket);
    m_output_sockets.append(&output_socket);
  }

  m_dummy_nodes.append(&node);
  m_node_by_id.append(&node);
  return node;
}

void MFNetworkBuilder::add_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to)
{
  BLI_assert(to.origin() == nullptr);
  BLI_assert(from.m_node->m_network == to.m_node->m_network);
  from.m_targets.append(&to);
  to.m_origin = &from;
}

std::string MFNetworkBuilder::to_dot()
{
  using BLI::DotExport::Utils::NodeWithSocketsWrapper;

  BLI::DotExport::DirectedGraph digraph;
  digraph.set_rankdir(BLI::DotExport::Attr_rankdir::LeftToRight);
  Map<MFBuilderNode *, NodeWithSocketsWrapper> dot_nodes;

  for (MFBuilderNode *node : m_node_by_id) {
    auto &dot_node = digraph.new_node("");

    Vector<std::string> input_names;
    for (MFBuilderInputSocket *socket : node->inputs()) {
      input_names.append(socket->name());
    }
    Vector<std::string> output_names;
    for (MFBuilderOutputSocket *socket : node->outputs()) {
      output_names.append(socket->name());
    }

    if (node->is_dummy()) {
      dot_node.set_background_color("#AAAAFF");
    }

    dot_nodes.add_new(node,
                      NodeWithSocketsWrapper(dot_node, node->name(), input_names, output_names));
  }

  for (MFBuilderNode *to_node : m_node_by_id) {
    auto to_dot_node = dot_nodes.lookup(to_node);

    for (MFBuilderInputSocket *to_socket : to_node->inputs()) {
      MFBuilderOutputSocket *from_socket = to_socket->origin();
      if (from_socket != nullptr) {
        MFBuilderNode &from_node = from_socket->node();

        auto from_dot_node = dot_nodes.lookup(&from_node);

        digraph.new_edge(from_dot_node.output(from_socket->index()),
                         to_dot_node.input(to_socket->index()));
      }
    }
  }

  return digraph.to_dot_string();
}

void MFNetworkBuilder::to_dot__clipboard()
{
  std::string dot = this->to_dot();
  WM_clipboard_text_set(dot.c_str(), false);
}

/* Network
 ********************************************/

MFNetwork::MFNetwork(std::unique_ptr<MFNetworkBuilder> builder)
{
  m_node_by_id = Array<MFNode *>(builder->nodes_by_id().size());
  m_socket_by_id = Array<MFSocket *>(builder->sockets_by_id().size());

  for (MFBuilderFunctionNode *builder_node : builder->function_nodes()) {
    MFFunctionNode &node = *m_allocator.construct<MFFunctionNode>().release();

    node.m_function = &builder_node->function();
    node.m_id = builder_node->id();
    node.m_input_param_indices = builder_node->input_param_indices();
    node.m_output_param_indices = builder_node->output_param_indices();
    node.m_network = this;
    node.m_is_dummy = false;

    for (MFBuilderInputSocket *builder_socket : builder_node->inputs()) {
      MFInputSocket &socket = *m_allocator.construct<MFInputSocket>().release();
      socket.m_id = builder_socket->id();
      socket.m_index = builder_socket->index();
      socket.m_is_output = false;
      socket.m_node = &node;
      socket.m_data_type = builder_socket->data_type();

      m_socket_by_id[socket.id()] = &socket;
      m_input_sockets.append(&socket);
      node.m_inputs.append(&socket);
    }
    for (MFBuilderOutputSocket *builder_socket : builder_node->outputs()) {
      MFOutputSocket &socket = *m_allocator.construct<MFOutputSocket>().release();
      socket.m_id = builder_socket->id();
      socket.m_index = builder_socket->index();
      socket.m_is_output = true;
      socket.m_node = &node;
      socket.m_data_type = builder_socket->data_type();

      m_socket_by_id[socket.id()] = &socket;
      m_output_sockets.append(&socket);
      node.m_outputs.append(&socket);
    }

    m_function_nodes.append(&node);
    m_node_by_id[node.id()] = &node;
  }

  for (MFBuilderDummyNode *builder_node : builder->dummy_nodes()) {
    MFDummyNode &node = *m_allocator.construct<MFDummyNode>().release();

    node.m_id = builder_node->id();
    node.m_network = this;
    node.m_is_dummy = true;

    for (MFBuilderInputSocket *builder_socket : builder_node->inputs()) {
      MFInputSocket &socket = *m_allocator.construct<MFInputSocket>().release();
      socket.m_id = builder_socket->id();
      socket.m_index = builder_socket->index();
      socket.m_is_output = false;
      socket.m_node = &node;
      socket.m_data_type = builder_socket->data_type();

      m_socket_by_id[socket.id()] = &socket;
      m_input_sockets.append(&socket);
      node.m_inputs.append(&socket);
      node.m_input_names.append(m_allocator.copy_string(builder_socket->name()));
    }
    for (MFBuilderOutputSocket *builder_socket : builder_node->outputs()) {
      MFOutputSocket &socket = *m_allocator.construct<MFOutputSocket>().release();
      socket.m_id = builder_socket->id();
      socket.m_index = builder_socket->index();
      socket.m_is_output = true;
      socket.m_node = &node;
      socket.m_data_type = builder_socket->data_type();

      m_socket_by_id[socket.id()] = &socket;
      m_output_sockets.append(&socket);
      node.m_outputs.append(&socket);
      node.m_output_names.append(m_allocator.copy_string(builder_socket->name()));
    }

    m_dummy_nodes.append(&node);
    m_node_by_id[node.id()] = &node;
  }

  for (MFBuilderInputSocket *builder_socket : builder->input_sockets()) {
    MFInputSocket &socket = m_socket_by_id[builder_socket->id()]->as_input();
    MFOutputSocket &origin = m_socket_by_id[builder_socket->origin()->id()]->as_output();

    socket.m_origin = &origin;
  }

  for (MFBuilderOutputSocket *builder_socket : builder->output_sockets()) {
    MFOutputSocket &socket = m_socket_by_id[builder_socket->id()]->as_output();

    for (MFBuilderInputSocket *builder_target : builder_socket->targets()) {
      MFInputSocket &target = m_socket_by_id[builder_target->id()]->as_input();
      socket.m_targets.append(&target);
    }
  }
}

MFNetwork::~MFNetwork()
{
  for (auto node : m_function_nodes) {
    node->~MFFunctionNode();
  }
  for (auto node : m_dummy_nodes) {
    node->~MFDummyNode();
  }
  for (auto socket : m_input_sockets) {
    socket->~MFInputSocket();
  }
  for (auto socket : m_output_sockets) {
    socket->~MFOutputSocket();
  }
}

Vector<const MFOutputSocket *> MFNetwork::find_dummy_dependencies(
    ArrayRef<const MFInputSocket *> sockets) const
{
  Vector<const MFOutputSocket *> dummy_dependencies;
  Set<const MFOutputSocket *> found_outputs;
  Stack<const MFInputSocket *> inputs_to_check = sockets;

  while (!inputs_to_check.empty()) {
    const MFInputSocket &input_socket = *inputs_to_check.pop();
    const MFOutputSocket &origin_socket = input_socket.origin();

    if (found_outputs.add(&origin_socket)) {
      const MFNode &origin_node = origin_socket.node();
      if (origin_node.is_dummy()) {
        dummy_dependencies.append(&origin_socket);
      }
      else {
        inputs_to_check.push_multiple(origin_node.inputs());
      }
    }
  }

  return dummy_dependencies;
}

Vector<const MFFunctionNode *> MFNetwork::find_function_dependencies(
    ArrayRef<const MFInputSocket *> sockets) const
{
  Vector<const MFFunctionNode *> function_dependencies;
  Set<const MFNode *> found_nodes;
  Stack<const MFInputSocket *> inputs_to_check = sockets;

  while (!inputs_to_check.empty()) {
    const MFInputSocket &input_socket = *inputs_to_check.pop();
    const MFOutputSocket &origin_socket = input_socket.origin();
    const MFNode &origin_node = origin_socket.node();

    if (found_nodes.add(&origin_node)) {
      if (origin_node.is_function()) {
        function_dependencies.append(&origin_node.as_function());
        inputs_to_check.push_multiple(origin_node.inputs());
      }
    }
  }

  return function_dependencies;
}

}  // namespace FN
