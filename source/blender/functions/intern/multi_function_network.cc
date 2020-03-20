#include <sstream>

#include "FN_multi_function_network.h"

#include "BLI_dot_export.h"
#include "BLI_set.h"
#include "BLI_stack_cxx.h"

extern "C" {
void WM_clipboard_text_set(const char *buf, bool selection);
}

namespace FN {

using BLI::Map;
using BLI::ScopedVector;
using BLI::Set;
using BLI::Stack;

/* MFNetwork Builder
 **************************************/

MFNetworkBuilder::~MFNetworkBuilder()
{
  for (MFBuilderFunctionNode *node : m_function_nodes) {
    for (MFBuilderInputSocket *input_socket : node->m_inputs) {
      input_socket->~MFBuilderInputSocket();
    }
    for (MFBuilderOutputSocket *output_socket : node->m_outputs) {
      output_socket->~MFBuilderOutputSocket();
    }
    node->~MFBuilderFunctionNode();
  }

  for (MFBuilderDummyNode *node : m_dummy_nodes) {
    for (MFBuilderInputSocket *input_socket : node->m_inputs) {
      input_socket->~MFBuilderInputSocket();
    }
    for (MFBuilderOutputSocket *output_socket : node->m_outputs) {
      output_socket->~MFBuilderOutputSocket();
    }
    node->~MFBuilderDummyNode();
  }
}

MFBuilderFunctionNode &MFNetworkBuilder::add_function(const MultiFunction &function)
{
  ScopedVector<uint> input_param_indices;
  ScopedVector<uint> output_param_indices;
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

  auto &node = *m_allocator.construct<MFBuilderFunctionNode>();
  m_function_nodes.add_new(&node);

  node.m_network = this;
  node.m_is_dummy = false;
  node.m_function = &function;
  node.m_id = m_node_or_null_by_id.append_and_get_index(&node);
  node.m_input_param_indices = m_allocator.construct_array_copy<uint>(input_param_indices);
  node.m_output_param_indices = m_allocator.construct_array_copy<uint>(output_param_indices);

  node.m_inputs = m_allocator.construct_elements_and_pointer_array<MFBuilderInputSocket>(
      input_param_indices.size());
  node.m_outputs = m_allocator.construct_elements_and_pointer_array<MFBuilderOutputSocket>(
      output_param_indices.size());

  for (uint i : input_param_indices.index_range()) {
    MFParamType param = function.param_type(input_param_indices[i]);
    BLI_assert(param.is_input_or_mutable());

    MFBuilderInputSocket &input_socket = *node.m_inputs[i];
    input_socket.m_data_type = param.data_type();
    input_socket.m_node = &node;
    input_socket.m_index = i;
    input_socket.m_is_output = false;
    input_socket.m_id = m_socket_or_null_by_id.append_and_get_index(&input_socket);
  }

  for (uint i : output_param_indices.index_range()) {
    MFParamType param = function.param_type(output_param_indices[i]);
    BLI_assert(param.is_output_or_mutable());

    MFBuilderOutputSocket &output_socket = *node.m_outputs[i];
    output_socket.m_data_type = param.data_type();
    output_socket.m_node = &node;
    output_socket.m_index = i;
    output_socket.m_is_output = true;
    output_socket.m_id = m_socket_or_null_by_id.append_and_get_index(&output_socket);
  }

  return node;
}

MFBuilderDummyNode &MFNetworkBuilder::add_dummy(StringRef name,
                                                ArrayRef<MFDataType> input_types,
                                                ArrayRef<MFDataType> output_types,
                                                ArrayRef<StringRef> input_names,
                                                ArrayRef<StringRef> output_names)
{
  BLI::assert_same_size(input_types, input_names);
  BLI::assert_same_size(output_types, output_names);

  auto &node = *m_allocator.construct<MFBuilderDummyNode>();
  m_dummy_nodes.add_new(&node);

  node.m_network = this;
  node.m_is_dummy = true;
  node.m_name = m_allocator.copy_string(name);
  node.m_id = m_node_or_null_by_id.append_and_get_index(&node);

  node.m_inputs = m_allocator.construct_elements_and_pointer_array<MFBuilderInputSocket>(
      input_types.size());
  node.m_outputs = m_allocator.construct_elements_and_pointer_array<MFBuilderOutputSocket>(
      output_types.size());

  node.m_input_names = m_allocator.allocate_array<StringRefNull>(input_types.size());
  node.m_output_names = m_allocator.allocate_array<StringRefNull>(output_types.size());

  for (uint i : input_types.index_range()) {
    MFBuilderInputSocket &input_socket = *node.m_inputs[i];
    input_socket.m_data_type = input_types[i];
    input_socket.m_node = &node;
    input_socket.m_index = i;
    input_socket.m_is_output = false;
    input_socket.m_id = m_socket_or_null_by_id.append_and_get_index(&input_socket);
    node.m_input_names[i] = m_allocator.copy_string(input_names[i]);
  }
  for (uint i : output_types.index_range()) {
    MFBuilderOutputSocket &output_socket = *node.m_outputs[i];
    output_socket.m_data_type = output_types[i];
    output_socket.m_node = &node;
    output_socket.m_index = i;
    output_socket.m_is_output = true;
    output_socket.m_id = m_socket_or_null_by_id.append_and_get_index(&output_socket);
    node.m_output_names[i] = m_allocator.copy_string(output_names[i]);
  }
  return node;
}

MFBuilderDummyNode &MFNetworkBuilder::add_input_dummy(StringRef name, MFBuilderInputSocket &socket)
{
  MFBuilderDummyNode &node = this->add_dummy(name, {}, {socket.data_type()}, {}, {"Value"});
  this->add_link(node.output(0), socket);
  return node;
}

MFBuilderDummyNode &MFNetworkBuilder::add_output_dummy(StringRef name,
                                                       MFBuilderOutputSocket &socket)
{
  MFBuilderDummyNode &node = this->add_dummy(name, {socket.data_type()}, {}, {"Value"}, {});
  this->add_link(socket, node.input(0));
  return node;
}

void MFNetworkBuilder::add_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to)
{
  BLI_assert(to.origin() == nullptr);
  BLI_assert(from.m_node->m_network == to.m_node->m_network);
  BLI_assert(from.data_type() == to.data_type());
  from.m_targets.append(&to, m_allocator);
  to.m_origin = &from;
}

void MFNetworkBuilder::remove_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to)
{
  BLI_assert(from.m_targets.contains(&to));
  BLI_assert(to.m_origin == &from);
  from.m_targets.remove_first_occurrence_and_reorder(&to);
  to.m_origin = nullptr;
}

void MFNetworkBuilder::replace_origin(MFBuilderOutputSocket &old_origin,
                                      MFBuilderOutputSocket &new_origin)
{
  BLI_assert(&old_origin != &new_origin);
  BLI_assert(old_origin.data_type() == new_origin.data_type());
  for (MFBuilderInputSocket *target : old_origin.targets()) {
    BLI_assert(target->m_origin != nullptr);
    target->m_origin = &new_origin;
    new_origin.m_targets.append(target, m_allocator);
  }
  old_origin.m_targets.clear();
}

void MFNetworkBuilder::remove_node(MFBuilderNode &node)
{
  for (MFBuilderInputSocket *input_socket : node.inputs()) {
    m_socket_or_null_by_id[input_socket->m_id] = nullptr;
    MFBuilderOutputSocket *origin = input_socket->origin();
    if (origin != nullptr) {
      origin->m_targets.remove_first_occurrence_and_reorder(input_socket);
    }
    input_socket->~MFBuilderInputSocket();
  }
  for (MFBuilderOutputSocket *output_socket : node.outputs()) {
    m_socket_or_null_by_id[output_socket->m_id] = nullptr;
    for (MFBuilderInputSocket *target : output_socket->targets()) {
      target->m_origin = nullptr;
    }
    output_socket->~MFBuilderOutputSocket();
  }

  m_node_or_null_by_id[node.m_id] = nullptr;
  if (node.is_dummy()) {
    MFBuilderDummyNode &dummy_node = node.as_dummy();
    m_dummy_nodes.remove(&dummy_node);
    dummy_node.~MFBuilderDummyNode();
  }
  else {
    MFBuilderFunctionNode &function_node = node.as_function();
    m_function_nodes.remove(&function_node);
    function_node.~MFBuilderFunctionNode();
  }
}

void MFNetworkBuilder::remove_nodes(ArrayRef<MFBuilderNode *> nodes)
{
  for (MFBuilderNode *node : nodes) {
    this->remove_node(*node);
  }
}

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

Array<bool> MFNetworkBuilder::find_nodes_to_the_right_of__inclusive__mask(
    ArrayRef<MFBuilderNode *> nodes)
{
  Array<bool> is_to_the_right(this->node_id_amount(), false);

  for (MFBuilderNode *node : nodes) {
    is_to_the_right[node->id()] = true;
  }

  Stack<MFBuilderNode *> nodes_to_check = nodes;
  while (!nodes_to_check.is_empty()) {
    MFBuilderNode &node = *nodes_to_check.pop();

    if (is_to_the_right[node.id()]) {
      node.foreach_target_node([&](MFBuilderNode &other_node) {
        if (set_tag_and_check_if_modified(is_to_the_right[other_node.id()], true)) {
          nodes_to_check.push(&other_node);
        }
      });
    }
  }

  return is_to_the_right;
}

Array<bool> MFNetworkBuilder::find_nodes_to_the_left_of__inclusive__mask(
    ArrayRef<MFBuilderNode *> nodes)
{
  Array<bool> is_to_the_left(this->node_id_amount(), false);

  for (MFBuilderNode *node : nodes) {
    is_to_the_left[node->id()] = true;
  }

  Stack<MFBuilderNode *> nodes_to_check = nodes;
  while (!nodes_to_check.is_empty()) {
    MFBuilderNode &node = *nodes_to_check.pop();

    if (is_to_the_left[node.id()]) {
      node.foreach_origin_node([&](MFBuilderNode &other_node) {
        if (set_tag_and_check_if_modified(is_to_the_left[other_node.id()], true)) {
          nodes_to_check.push(&other_node);
        }
      });
    }
  }

  return is_to_the_left;
}

Vector<MFBuilderNode *> MFNetworkBuilder::nodes_by_id_inverted_id_mask(ArrayRef<bool> id_mask)
{
  Vector<MFBuilderNode *> nodes;
  for (uint id : id_mask.index_range()) {
    if (this->node_id_is_valid(id)) {
      if (!id_mask[id]) {
        MFBuilderNode &node = this->node_by_id(id);
        nodes.append(&node);
      }
    }
  }
  return nodes;
}

Vector<MFBuilderNode *> MFNetworkBuilder::find_nodes_not_to_the_left_of__exclusive__vector(
    ArrayRef<MFBuilderNode *> nodes)
{
  Array<bool> is_to_the_left = this->find_nodes_to_the_left_of__inclusive__mask(nodes);
  Vector<MFBuilderNode *> result = this->nodes_by_id_inverted_id_mask(is_to_the_left);
  return result;
}

std::string MFNetworkBuilder::to_dot(const Set<MFBuilderNode *> &marked_nodes)
{
  using BLI::DotExport::NodeWithSocketsRef;

  BLI::DotExport::DirectedGraph digraph;
  digraph.set_rankdir(BLI::DotExport::Attr_rankdir::LeftToRight);
  Map<MFBuilderNode *, NodeWithSocketsRef> dot_nodes;

  Vector<MFBuilderNode *> all_nodes;
  all_nodes.extend(m_function_nodes.as_ref());
  all_nodes.extend(m_dummy_nodes.as_ref());

  for (MFBuilderNode *node : all_nodes) {
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
      dot_node.set_background_color("#DDDDFF");
    }
    if (marked_nodes.contains(node)) {
      dot_node.set_background_color("#99EE99");
    }

    dot_nodes.add_new(node, NodeWithSocketsRef(dot_node, node->name(), input_names, output_names));
  }

  for (MFBuilderNode *to_node : all_nodes) {
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

void MFNetworkBuilder::to_dot__clipboard(const Set<MFBuilderNode *> &marked_nodes)
{
  std::string dot = this->to_dot(marked_nodes);
  WM_clipboard_text_set(dot.c_str(), false);
}

/* Network
 ********************************************/

MFNetwork::MFNetwork(MFNetworkBuilder &builder)
{
  ArrayRef<MFBuilderFunctionNode *> builder_function_nodes = builder.function_nodes();
  ArrayRef<MFBuilderDummyNode *> builder_dummy_nodes = builder.dummy_nodes();

  for (MFBuilderFunctionNode *builder_node : builder_function_nodes) {
    uint input_amount = builder_node->inputs().size();
    uint output_amount = builder_node->outputs().size();

    MFFunctionNode &node = *m_allocator.construct<MFFunctionNode>();

    node.m_function = &builder_node->function();
    node.m_id = m_node_by_id.append_and_get_index(&node);
    node.m_network = this;
    node.m_is_dummy = false;

    node.m_input_param_indices = m_allocator.construct_array_copy(
        builder_node->input_param_indices());
    node.m_output_param_indices = m_allocator.construct_array_copy(
        builder_node->output_param_indices());

    node.m_inputs = m_allocator.construct_elements_and_pointer_array<MFInputSocket>(input_amount);
    node.m_outputs = m_allocator.construct_elements_and_pointer_array<MFOutputSocket>(
        output_amount);

    for (uint i : IndexRange(input_amount)) {
      MFBuilderInputSocket &builder_socket = builder_node->input(i);
      MFInputSocket &socket = *node.m_inputs[i];
      socket.m_id = m_socket_by_id.append_and_get_index(&socket);
      socket.m_index = i;
      socket.m_is_output = false;
      socket.m_node = &node;
      socket.m_data_type = builder_socket.data_type();

      m_input_sockets.append(&socket);
    }
    for (uint i : IndexRange(output_amount)) {
      MFBuilderOutputSocket &builder_socket = builder_node->output(i);
      MFOutputSocket &socket = *node.m_outputs[i];
      socket.m_id = m_socket_by_id.append_and_get_index(&socket);
      socket.m_index = i;
      socket.m_is_output = true;
      socket.m_node = &node;
      socket.m_data_type = builder_socket.data_type();

      m_output_sockets.append(&socket);
    }

    m_function_nodes.append(&node);
  }

  for (MFBuilderDummyNode *builder_node : builder_dummy_nodes) {
    uint input_amount = builder_node->inputs().size();
    uint output_amount = builder_node->outputs().size();

    MFDummyNode &node = *m_allocator.construct<MFDummyNode>();

    node.m_id = m_node_by_id.append_and_get_index(&node);
    node.m_network = this;
    node.m_is_dummy = true;

    node.m_inputs = m_allocator.construct_elements_and_pointer_array<MFInputSocket>(input_amount);
    node.m_outputs = m_allocator.construct_elements_and_pointer_array<MFOutputSocket>(
        output_amount);

    node.m_input_names = m_allocator.allocate_array<StringRefNull>(input_amount);
    node.m_output_names = m_allocator.allocate_array<StringRefNull>(output_amount);

    for (uint i : IndexRange(input_amount)) {
      MFBuilderInputSocket &builder_socket = builder_node->input(i);
      MFInputSocket &socket = *node.m_inputs[i];
      socket.m_id = m_socket_by_id.append_and_get_index(&socket);
      socket.m_index = i;
      socket.m_is_output = false;
      socket.m_node = &node;
      socket.m_data_type = builder_socket.data_type();

      m_input_sockets.append(&socket);
      node.m_input_names[i] = m_allocator.copy_string(builder_socket.name());
    }
    for (uint i : IndexRange(output_amount)) {
      MFBuilderOutputSocket &builder_socket = builder_node->output(i);
      MFOutputSocket &socket = *node.m_outputs[i];
      socket.m_id = m_socket_by_id.append_and_get_index(&socket);
      socket.m_index = i;
      socket.m_is_output = true;
      socket.m_node = &node;
      socket.m_data_type = builder_socket.data_type();

      m_output_sockets.append(&socket);
      node.m_output_names[i] = m_allocator.copy_string(builder_socket.name());
    }

    m_dummy_nodes.append(&node);
  }

  for (uint node_index : builder_function_nodes.index_range()) {
    MFFunctionNode *to_node = m_function_nodes[node_index];
    MFBuilderFunctionNode *to_builder_node = builder_function_nodes[node_index];
    this->create_links_to_node(builder, to_node, to_builder_node);
  }
  for (uint node_index : builder.dummy_nodes().index_range()) {
    MFDummyNode *to_node = m_dummy_nodes[node_index];
    MFBuilderDummyNode *to_builder_node = builder_dummy_nodes[node_index];
    this->create_links_to_node(builder, to_node, to_builder_node);
  }

  this->compute_max_dependency_depths();
}

void MFNetwork::create_links_to_node(MFNetworkBuilder &builder,
                                     MFNode *to_node,
                                     MFBuilderNode *to_builder_node)
{
  for (uint socket_index : to_builder_node->inputs().index_range()) {
    MFInputSocket *to_socket = to_node->m_inputs[socket_index];
    MFBuilderInputSocket *to_builder_socket = &to_builder_node->input(socket_index);
    this->create_link_to_socket(builder, to_socket, to_builder_socket);
  }
}

void MFNetwork::create_link_to_socket(MFNetworkBuilder &builder,
                                      MFInputSocket *to_socket,
                                      MFBuilderInputSocket *to_builder_socket)
{
  BLI_assert(to_socket->m_origin == nullptr);

  MFBuilderOutputSocket *from_builder_socket = to_builder_socket->origin();
  BLI_assert(from_builder_socket != nullptr);

  MFBuilderNode *from_builder_node = &from_builder_socket->node();

  MFNode *from_node = nullptr;
  if (from_builder_node->is_dummy()) {
    uint dummy_node_index = builder.current_index_of(from_builder_node->as_dummy());
    from_node = m_dummy_nodes[dummy_node_index];
  }
  else {
    uint function_node_index = builder.current_index_of(from_builder_node->as_function());
    from_node = m_function_nodes[function_node_index];
  }

  uint from_index = from_builder_socket->index();
  MFOutputSocket *from_socket = from_node->m_outputs[from_index];

  from_socket->m_targets.append(to_socket);
  to_socket->m_origin = from_socket;
}

BLI_NOINLINE void MFNetwork::compute_max_dependency_depths()
{
  m_max_dependency_depth_per_node = Array<uint>(m_node_by_id.size(), UINT32_MAX);
  Array<uint> &max_depths = m_max_dependency_depth_per_node;

  for (const MFDummyNode *node : this->dummy_nodes()) {
    max_depths[node->id()] = 0;
  }

  Stack<const MFNode *> nodes_to_check;
  nodes_to_check.push_multiple(this->function_nodes());

  while (!nodes_to_check.is_empty()) {
    const MFNode &current = *nodes_to_check.peek();
    if (max_depths[current.id()] != UINT32_MAX) {
      nodes_to_check.pop();
      continue;
    }

    bool all_inputs_computed = true;
    uint max_incoming_depth = 0;
    current.foreach_origin_node([&](const MFNode &origin_node) {
      uint origin_depth = max_depths[origin_node.id()];
      if (origin_depth == UINT32_MAX) {
        nodes_to_check.push(&origin_node);
        all_inputs_computed = false;
      }
      else {
        max_incoming_depth = std::max(max_incoming_depth, origin_depth);
      }
    });

    if (!all_inputs_computed) {
      continue;
    }

    nodes_to_check.pop();
    max_depths[current.id()] = max_incoming_depth + 1;
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

  while (!inputs_to_check.is_empty()) {
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

  while (!inputs_to_check.is_empty()) {
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
