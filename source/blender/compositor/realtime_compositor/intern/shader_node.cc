/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_vector.h"
#include "BLI_string_ref.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

#include "GPU_material.h"

#include "COM_shader_node.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

ShaderNode::ShaderNode(DNode node) : node_(node)
{
  populate_inputs();
  populate_outputs();
}

GPUNodeStack *ShaderNode::get_inputs_array()
{
  return inputs_.data();
}

GPUNodeStack *ShaderNode::get_outputs_array()
{
  return outputs_.data();
}

GPUNodeStack &ShaderNode::get_input(StringRef identifier)
{
  return inputs_[bke::node::socket_index_in_node(*node_.input_by_identifier(identifier))];
}

GPUNodeStack &ShaderNode::get_output(StringRef identifier)
{
  return outputs_[bke::node::socket_index_in_node(*node_.output_by_identifier(identifier))];
}

GPUNodeLink *ShaderNode::get_input_link(StringRef identifier)
{
  GPUNodeStack &input = get_input(identifier);
  if (input.link) {
    return input.link;
  }
  return GPU_uniform(input.vec);
}

const DNode &ShaderNode::node() const
{
  return node_;
}

bNode &ShaderNode::bnode() const
{
  return const_cast<bNode &>(*node_);
}

static eGPUType gpu_type_from_socket_type(eNodeSocketDatatype type)
{
  switch (type) {
    case SOCK_FLOAT:
      return GPU_FLOAT;
    case SOCK_VECTOR:
      return GPU_VEC3;
    case SOCK_RGBA:
      return GPU_VEC4;
    default:
      BLI_assert_unreachable();
      return GPU_NONE;
  }
}

static void gpu_stack_vector_from_socket(float *vector, const bNodeSocket *socket)
{
  switch (socket->type) {
    case SOCK_FLOAT:
      vector[0] = bke::node::socket_default_value<bNodeSocketValueFloat>(*socket)->value;
      return;
    case SOCK_VECTOR:
      copy_v3_v3(vector, bke::node::socket_default_value<bNodeSocketValueVector>(*socket)->value);
      return;
    case SOCK_RGBA:
      copy_v4_v4(vector, bke::node::socket_default_value<bNodeSocketValueRGBA>(*socket)->value);
      return;
    default:
      BLI_assert_unreachable();
  }
}

static void populate_gpu_node_stack(DSocket socket, GPUNodeStack &stack)
{
  /* Make sure this stack is not marked as the end of the stack array. */
  stack.end = false;
  /* This will be initialized later by the GPU material compiler or the compile method. */
  stack.link = nullptr;

  stack.sockettype = socket->type;
  stack.type = gpu_type_from_socket_type((eNodeSocketDatatype)socket->type);

  if (socket->in_out == SOCK_IN) {
    const DInputSocket input(socket);

    DSocket origin = get_input_origin_socket(input);

    /* The input is linked if the origin socket is an output socket. Had it been an input socket,
     * then it is an unlinked input of a group input node. */
    stack.hasinput = origin->in_out == SOCK_OUT;

    /* Get the socket value from the origin if it is an input, because then it would either be an
     * unlinked input or an unlinked input of a group input node that the socket is linked to,
     * otherwise, get the value from the socket itself. */
    if (origin->in_out == SOCK_IN) {
      gpu_stack_vector_from_socket(stack.vec, origin.bsocket());
    }
    else {
      gpu_stack_vector_from_socket(stack.vec, socket.bsocket());
    }
  }
  else {
    stack.hasoutput = !bke::node::logically_linked_sockets(*socket).is_empty();
  }
}

void ShaderNode::populate_inputs()
{
  /* Reserve a stack for each input in addition to an extra stack at the end to mark the end of the
   * array, as this is what the GPU module functions expect. */
  const int num_input_sockets = bke::node::node_inputs(*node_).size();
  inputs_.resize(num_input_sockets + 1);
  inputs_.last().end = true;

  for (int i = 0; i < num_input_sockets; i++) {
    populate_gpu_node_stack(node_.input(i), inputs_[i]);
  }
}

void ShaderNode::populate_outputs()
{
  /* Reserve a stack for each output in addition to an extra stack at the end to mark the end of
   * the array, as this is what the GPU module functions expect. */
  const int num_output_sockets = bke::node::node_outputs(*node_).size();
  outputs_.resize(num_output_sockets + 1);
  outputs_.last().end = true;

  for (int i = 0; i < num_output_sockets; i++) {
    populate_gpu_node_stack(node_.output(i), outputs_[i]);
  }
}

}  // namespace blender::realtime_compositor
