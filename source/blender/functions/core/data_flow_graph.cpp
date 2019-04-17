#include "data_flow_graph.hpp"

namespace FN {
SharedType &Socket::type() const
{
  if (m_is_output) {
    return this->node()->signature().outputs()[m_index].type();
  }
  else {
    return this->node()->signature().inputs()[m_index].type();
  }
}

std::string Socket::name() const
{
  if (m_is_output) {
    return this->node()->signature().outputs()[m_index].name();
  }
  else {
    return this->node()->signature().inputs()[m_index].name();
  }
}

DataFlowGraph::DataFlowGraph() : m_node_pool(sizeof(Node))
{
}

DataFlowGraph::~DataFlowGraph()
{
  for (Node *node : m_nodes) {
    node->~Node();
  }
}

Node *DataFlowGraph::insert(SharedFunction &function, SourceInfo *source)
{
  BLI_assert(this->can_modify());

  void *ptr = m_node_pool.allocate();
  Node *node = new (ptr) Node(this, function, source);
  m_nodes.add(node);
  return node;
}

void DataFlowGraph::link(Socket a, Socket b)
{
  BLI_assert(this->can_modify());
  BLI_assert(a.node() != b.node());
  BLI_assert(a.type() == b.type());
  BLI_assert(a.is_input() != b.is_input());
  BLI_assert(a.graph() == this && b.graph() == this);

  m_links.insert(Link::New(a, b));
}

SocketSet FunctionGraph::find_used_sockets(bool include_inputs, bool include_outputs) const
{
  SocketSet found;

  SocketSet to_be_checked;
  for (Socket socket : m_outputs) {
    to_be_checked.add_new(socket);
  }

  while (to_be_checked.size() > 0) {
    Socket socket = to_be_checked.pop();

    if (!include_inputs && m_inputs.contains(socket)) {
      continue;
    }

    found.add(socket);

    if (socket.is_input()) {
      to_be_checked.add_new(socket.origin());
    }
    else {
      for (Socket input : socket.node()->inputs()) {
        to_be_checked.add_new(input);
      }
    }
  }

  if (!include_outputs) {
    for (Socket socket : m_outputs) {
      found.remove(socket);
    }
  }

  return found;
}

};  // namespace FN
