#include "FN_data_flow_nodes.hpp"

#include "BLI_stack.hpp"

namespace FN {
namespace DataFlowNodes {

using BLI::Stack;

VTreeDataGraph::PlaceholderDependencies VTreeDataGraph::find_placeholder_dependencies(
    ArrayRef<VirtualSocket *> vsockets)
{
  Vector<DFGraphSocket> sockets;
  for (VirtualSocket *vsocket : vsockets) {
    DFGraphSocket socket = this->lookup_socket(vsocket);
    sockets.append(socket);
  }
  return this->find_placeholder_dependencies(sockets);
}

VTreeDataGraph::PlaceholderDependencies VTreeDataGraph::find_placeholder_dependencies(
    ArrayRef<DFGraphSocket> sockets)
{
  Stack<DFGraphSocket> to_be_checked = sockets;
  Set<DFGraphSocket> found = sockets;
  PlaceholderDependencies dependencies;

  while (!to_be_checked.empty()) {
    DFGraphSocket socket = to_be_checked.pop();
    if (socket.is_input()) {
      DFGraphSocket origin = m_graph->origin_of_input(socket);
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
        dependencies.sockets.append(socket);
        dependencies.vsockets.append(vsocket);
      }
      else {
        for (DFGraphSocket input : m_graph->inputs_of_node(node_id)) {
          if (found.add(input)) {
            to_be_checked.push(input);
          }
        }
      }
    }
  }

  return dependencies;
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
