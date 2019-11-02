#include "BKE_virtual_node_tree.h"

#include "BLI_listbase_wrapper.h"
#include "BLI_map.h"

namespace BKE {

using BLI::Map;
using BSocketList = BLI::IntrusiveListBaseWrapper<bNodeSocket>;
using BNodeList = BLI::IntrusiveListBaseWrapper<bNode>;
using BLinkList = BLI::IntrusiveListBaseWrapper<bNodeLink>;

VirtualNodeTreeBuilder::~VirtualNodeTreeBuilder()
{
  for (VBNode *node : m_nodes_by_id) {
    delete node;
  }
  for (VBInputSocket *socket : m_input_sockets) {
    delete socket;
  }
  for (VBOutputSocket *socket : m_output_sockets) {
    delete socket;
  }
}

VBNode &VirtualNodeTreeBuilder::add_node(bNodeTree *btree, bNode *bnode)
{
  VBNode *node = new VBNode();
  node->m_vtree = this;
  node->m_id = m_nodes_by_id.size();
  node->m_btree = btree;
  node->m_bnode = bnode;

  for (bNodeSocket *bsocket : BSocketList(bnode->inputs)) {
    VBInputSocket *socket = new VBInputSocket();
    socket->m_node = node;
    socket->m_is_input = true;
    socket->m_id = m_sockets_by_id.size();
    socket->m_btree = btree;
    socket->m_bsocket = bsocket;
    socket->m_index = node->m_inputs.size();

    m_input_sockets.append(socket);
    m_sockets_by_id.append(socket);
    node->m_inputs.append(socket);
  }

  for (bNodeSocket *bsocket : BSocketList(bnode->outputs)) {
    VBOutputSocket *socket = new VBOutputSocket();
    socket->m_node = node;
    socket->m_is_input = false;
    socket->m_id = m_sockets_by_id.size();
    socket->m_btree = btree;
    socket->m_bsocket = bsocket;
    socket->m_index = node->m_outputs.size();

    m_output_sockets.append(socket);
    m_sockets_by_id.append(socket);
    node->m_outputs.append(socket);
  }

  m_nodes_by_id.append(node);
  return *node;
}

void VirtualNodeTreeBuilder::add_link(VBOutputSocket &from, VBInputSocket &to)
{
  VBLink *link = new VBLink();
  link->m_from = &from;
  link->m_to = &to;
  m_links.append(link);
}

void VirtualNodeTreeBuilder::add_all_of_node_tree(bNodeTree *btree)
{
  Map<bNode *, VBNode *> node_mapping;
  for (bNode *bnode : BNodeList(btree->nodes)) {
    VBNode &vnode = this->add_node(btree, bnode);
    node_mapping.add_new(bnode, &vnode);
  }

  for (bNodeLink *blink : BLinkList(btree->links)) {
    VBNode &from_vnode = *node_mapping.lookup(blink->fromnode);
    VBNode &to_vnode = *node_mapping.lookup(blink->tonode);

    uint from_socket_index = BSocketList(blink->fromnode->outputs).index_of(blink->fromsock);
    uint to_socket_index = BSocketList(blink->tonode->inputs).index_of(blink->tosock);

    VBOutputSocket &from_socket = *from_vnode.outputs()[from_socket_index];
    VBInputSocket &to_socket = *to_vnode.inputs()[to_socket_index];

    this->add_link(from_socket, to_socket);
  }
}

std::unique_ptr<VirtualNodeTree> VirtualNodeTreeBuilder::build()
{
  VirtualNodeTree *vtree = new VirtualNodeTree();

  vtree->m_nodes_by_id = Array<VNode *>(m_nodes_by_id.size());
  vtree->m_sockets_by_id = Array<VSocket *>(m_sockets_by_id.size());

  this->build__copy_nodes_and_sockets(*vtree);
  this->build__copy_direct_links(*vtree);
  this->build__setup_links_skipping_reroutes(*vtree);
  this->build__create_idname_to_nodes_mapping(*vtree);

  return std::unique_ptr<VirtualNodeTree>(vtree);
}

void VirtualNodeTreeBuilder::build__copy_nodes_and_sockets(VirtualNodeTree &vtree)
{
  for (VBNode *vbnode : m_nodes_by_id) {
    VNode *vnode = new VNode();
    vnode->m_bnode = vbnode->m_bnode;
    vnode->m_btree = vbnode->m_btree;
    vnode->m_id = vbnode->m_id;
    vnode->m_vtree = &vtree;
    RNA_pointer_create((ID *)vnode->m_btree, &RNA_Node, vnode->m_bnode, &vnode->m_rna);

    for (VBInputSocket *vbsocket : vbnode->m_inputs) {
      VInputSocket *vsocket = new VInputSocket();
      vsocket->m_bsocket = vbsocket->m_bsocket;
      vsocket->m_btree = vbsocket->m_btree;
      vsocket->m_id = vbsocket->m_id;
      vsocket->m_index = vbsocket->m_index;
      vsocket->m_node = vnode;
      vsocket->m_is_input = true;
      RNA_pointer_create(
          (ID *)vsocket->m_btree, &RNA_NodeSocket, vsocket->m_bsocket, &vsocket->m_rna);

      vnode->m_inputs.append(vsocket);
      vtree.m_sockets_by_id[vsocket->m_id] = vsocket;
      vtree.m_input_sockets.append(vsocket);
    }

    for (VBOutputSocket *vbsocket : vbnode->m_outputs) {
      VOutputSocket *vsocket = new VOutputSocket();
      vsocket->m_bsocket = vbsocket->m_bsocket;
      vsocket->m_btree = vbsocket->m_btree;
      vsocket->m_id = vbsocket->m_id;
      vsocket->m_index = vbsocket->m_index;
      vsocket->m_node = vnode;
      vsocket->m_is_input = false;
      RNA_pointer_create(
          (ID *)vsocket->m_btree, &RNA_NodeSocket, vsocket->m_bsocket, &vsocket->m_rna);

      vnode->m_outputs.append(vsocket);
      vtree.m_sockets_by_id[vsocket->m_id] = vsocket;
      vtree.m_output_sockets.append(vsocket);
    }

    vtree.m_nodes_by_id[vnode->m_id] = vnode;
  }
}

void VirtualNodeTreeBuilder::build__copy_direct_links(VirtualNodeTree &vtree)
{
  for (VBLink *vblink : m_links) {
    uint from_id = vblink->m_from->m_id;
    uint to_id = vblink->m_to->m_id;

    VSocket &from_vsocket = *vtree.m_sockets_by_id[from_id];
    VSocket &to_vsocket = *vtree.m_sockets_by_id[to_id];

    from_vsocket.m_directly_linked_sockets.append(&to_vsocket);
    to_vsocket.m_directly_linked_sockets.append(&from_vsocket);
  }
}

static bool is_reroute_node(const VNode &vnode)
{
  return vnode.idname() == "NodeReroute";
}

void VirtualNodeTreeBuilder::build__setup_links_skipping_reroutes(VirtualNodeTree &vtree)
{
  for (VOutputSocket *socket : vtree.m_output_sockets) {
    if (!is_reroute_node(socket->node())) {
      this->build__find_targets_skipping_reroutes(*socket, socket->m_linked_sockets);
      for (VSocket *target : socket->m_linked_sockets) {
        target->m_linked_sockets.append(socket);
      }
    }
  }
}

void VirtualNodeTreeBuilder::build__find_targets_skipping_reroutes(VOutputSocket &vsocket,
                                                                   Vector<VSocket *> &r_targets)
{
  for (VSocket *direct_target : vsocket.m_directly_linked_sockets) {
    if (is_reroute_node(*direct_target->m_node)) {
      this->build__find_targets_skipping_reroutes(*direct_target->m_node->m_outputs[0], r_targets);
    }
    else if (!r_targets.contains(direct_target)) {
      r_targets.append(direct_target);
    }
  }
}

void VirtualNodeTreeBuilder::build__create_idname_to_nodes_mapping(VirtualNodeTree &vtree)
{
  for (VNode *vnode : vtree.m_nodes_by_id) {
    if (vtree.m_nodes_by_idname.contains(vnode->idname())) {
      vtree.m_nodes_by_idname.lookup(vnode->idname()).append(vnode);
    }
    else {
      vtree.m_nodes_by_idname.add_new(vnode->idname(), {vnode});
    }
  }
}

VirtualNodeTree::~VirtualNodeTree()
{
  for (VNode *node : m_nodes_by_id) {
    delete node;
  }
  for (VInputSocket *socket : m_input_sockets) {
    delete socket;
  }
  for (VOutputSocket *socket : m_output_sockets) {
    delete socket;
  }
}

};  // namespace BKE
