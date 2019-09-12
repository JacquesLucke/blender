#include "FN_data_flow_nodes.hpp"

#include "BLI_stack_cxx.h"

namespace FN {
namespace DataFlowNodes {

using BLI::Stack;

VTreeDataGraph::VTreeDataGraph(VirtualNodeTree &vtree,
                               SharedDataGraph graph,
                               Array<DataSocket> mapping)
    : m_vtree(vtree), m_graph(std::move(graph)), m_socket_map(std::move(mapping))
{
}

Vector<VirtualSocket *> VTreeDataGraph::find_placeholder_dependencies(
    ArrayRef<VirtualSocket *> vsockets)
{
  Vector<DataSocket> sockets;
  for (VirtualSocket *vsocket : vsockets) {
    DataSocket socket = this->lookup_socket(vsocket);
    sockets.append(socket);
  }
  return this->find_placeholder_dependencies(sockets);
}

Vector<VirtualSocket *> VTreeDataGraph::find_placeholder_dependencies(ArrayRef<DataSocket> sockets)
{
  Stack<DataSocket> to_be_checked = sockets;
  Set<DataSocket> found = sockets;
  Vector<VirtualSocket *> vsocket_dependencies;

  while (!to_be_checked.empty()) {
    DataSocket socket = to_be_checked.pop();
    if (socket.is_input()) {
      DataSocket origin = m_graph->origin_of_input(socket);
      if (found.add(origin)) {
        to_be_checked.push(origin);
      }
    }
    else {
      uint node_id = m_graph->node_id_of_output(socket);
      SharedFunction &fn = m_graph->function_of_node(node_id);
      if (fn->has_body<VNodePlaceholderBody>()) {
        auto &body = fn->body<VNodePlaceholderBody>();
        VirtualNode *vnode = body.vnode();
        uint data_output_index = m_graph->index_of_output(socket);
        VirtualSocket *vsocket = this->find_data_output(vnode, data_output_index);
        vsocket_dependencies.append(vsocket);
      }
      else {
        for (DataSocket input : m_graph->inputs_of_node(node_id)) {
          if (found.add(input)) {
            to_be_checked.push(input);
          }
        }
      }
    }
  }

  return vsocket_dependencies;
}

VirtualSocket *VTreeDataGraph::find_data_output(VirtualNode *vnode, uint index)
{
  uint count = 0;
  for (uint i = 0; i < vnode->outputs().size(); i++) {
    VirtualSocket *vsocket = vnode->output(i);
    if (this->uses_socket(vsocket)) {
      if (index == count) {
        return vsocket;
      }
      count++;
    }
  }
  BLI_assert(false);
  return nullptr;
}

}  // namespace DataFlowNodes
}  // namespace FN
