#include "FN_core.hpp"

namespace FN {

DataGraphBuilder::DataGraphBuilder() : m_source_info_allocator(new MonotonicAllocator<>())
{
}

DataGraphBuilder::~DataGraphBuilder()
{
  /* Destruct source info if it is still owned. */
  if (m_source_info_allocator.get()) {
    for (BuilderNode *node : m_nodes) {
      if (node->source_info()) {
        node->source_info()->~SourceInfo();
      }
    }
  }
  for (BuilderNode *node : m_nodes) {
    node->~BuilderNode();
  }
}

BuilderNode *DataGraphBuilder::insert_function(SharedFunction function, SourceInfo *source_info)
{
  /* Allocate memory for node, input sockets and output sockets. */
  BuilderNode *node = m_allocator.allocate<BuilderNode>();

  ArrayRef<BuilderInputSocket> input_sockets = m_allocator.allocate_array<BuilderInputSocket>(
      function->input_amount());
  ArrayRef<BuilderInputSocket *> input_socket_pointers =
      m_allocator.allocate_array<BuilderInputSocket *>(function->input_amount());

  ArrayRef<BuilderOutputSocket> output_sockets = m_allocator.allocate_array<BuilderOutputSocket>(
      function->output_amount());
  ArrayRef<BuilderOutputSocket *> output_socket_pointers =
      m_allocator.allocate_array<BuilderOutputSocket *>(function->output_amount());

  /* Initialize input sockets. */
  for (uint i = 0; i < input_sockets.size(); i++) {
    BuilderInputSocket &socket = input_sockets[i];
    input_socket_pointers[i] = &socket;
    socket.m_input_id = m_input_socket_counter++;
    socket.m_node = node;
    socket.m_index = i;
    socket.m_is_input = true;
    socket.m_origin = nullptr;
  }

  /* Initialize output sockets. */
  for (uint i = 0; i < output_sockets.size(); i++) {
    BuilderOutputSocket &socket = output_sockets[i];
    output_socket_pointers[i] = &socket;
    socket.m_output_id = m_output_socket_counter++;
    socket.m_node = node;
    socket.m_index = i;
    socket.m_is_input = false;
    socket.m_targets = {};
  }

  /* Initialize node. */
  new (node) BuilderNode();
  node->m_id = m_nodes.size();
  node->m_function = std::move(function);
  node->m_builder = this;
  node->m_source_info = source_info;
  node->m_inputs = input_socket_pointers;
  node->m_outputs = output_socket_pointers;

  m_nodes.append(node);
  return node;
}

void DataGraphBuilder::insert_link(BuilderOutputSocket *from, BuilderInputSocket *to)
{
  BLI_assert(to->origin() == nullptr);
  to->m_origin = from;
  if (from->m_targets.is_full()) {
    uint old_capacity = from->m_targets.capacity();
    uint new_capacity = (old_capacity == 0) ? 1 : old_capacity * 2;
    ArrayRef<BuilderInputSocket *> new_targets = m_allocator.allocate_array<BuilderInputSocket *>(
        new_capacity);
    new_targets.take_front(old_capacity).copy_from(from->m_targets);
    from->m_targets = VectorAdaptor<BuilderInputSocket *>(
        new_targets.begin(), new_capacity, old_capacity);
  }
  from->m_targets.append(to);
  m_link_counter++;
}

SharedDataFlowGraph DataGraphBuilder::build()
{
  /* Every input socket should be linked to exactly one output. */
  BLI_assert(m_link_counter == m_input_socket_counter);

  Vector<DataFlowGraph::Node> r_nodes;
  r_nodes.reserve(m_nodes.size());
  Vector<DataFlowGraph::InputSocket> r_inputs;
  r_inputs.reserve(m_input_socket_counter);
  Vector<DataFlowGraph::OutputSocket> r_outputs;
  r_outputs.reserve(m_output_socket_counter);
  Vector<uint> r_targets;
  r_targets.reserve(m_link_counter);

  for (BuilderNode *builder_node : m_nodes) {
    uint node_id = builder_node->id();
    r_nodes.append(DataFlowGraph::Node(std::move(builder_node->function()),
                                       builder_node->source_info(),
                                       r_inputs.size(),
                                       r_outputs.size()));

    for (BuilderInputSocket *builder_socket : builder_node->inputs()) {
      BLI_assert(builder_socket->origin() != nullptr);
      uint origin_id = builder_socket->origin()->output_id();
      r_inputs.append(DataFlowGraph::InputSocket(node_id, origin_id));
    }

    for (BuilderOutputSocket *builder_socket : builder_node->outputs()) {
      auto targets = builder_socket->targets();
      r_outputs.append(DataFlowGraph::OutputSocket(node_id, r_targets.size(), targets.size()));

      for (BuilderInputSocket *target : targets) {
        r_targets.append(target->input_id());
      }
    }
  }

  return SharedDataFlowGraph::New(std::move(r_nodes),
                                  std::move(r_inputs),
                                  std::move(r_outputs),
                                  std::move(r_targets),
                                  std::move(m_source_info_allocator));
}

}  // namespace FN
