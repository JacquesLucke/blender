#include "BKE_virtual_node_tree.h"

#include "BLI_listbase_wrapper.h"
#include "BLI_map.h"

namespace BKE {

using BLI::Map;
using BSocketList = BLI::IntrusiveListBaseWrapper<bNodeSocket>;
using BNodeList = BLI::IntrusiveListBaseWrapper<bNode>;
using BLinkList = BLI::IntrusiveListBaseWrapper<bNodeLink>;

static bool is_reroute_node(const VNode &vnode)
{
  return vnode.idname() == "NodeReroute";
}

std::unique_ptr<VirtualNodeTree> VirtualNodeTree::FromBTree(bNodeTree *btree)
{
  BLI_assert(btree != nullptr);

  VirtualNodeTree *vtree_ptr = new VirtualNodeTree();
  VirtualNodeTree &vtree = *vtree_ptr;

  Map<bNode *, VNode *> node_mapping;

  for (bNode *bnode : BNodeList(btree->nodes)) {
    VNode &vnode = *vtree.m_allocator.construct<VNode>().release();

    vnode.m_vtree = &vtree;
    vnode.m_bnode = bnode;
    vnode.m_btree = btree;
    vnode.m_id = vtree.m_nodes_by_id.size();
    RNA_pointer_create(&btree->id, &RNA_Node, bnode, &vnode.m_rna);

    for (bNodeSocket *bsocket : BSocketList(bnode->inputs)) {
      VInputSocket &vsocket = *vtree.m_allocator.construct<VInputSocket>().release();

      vsocket.m_node = &vnode;
      vsocket.m_index = vnode.m_inputs.size();
      vsocket.m_is_input = true;
      vsocket.m_bsocket = bsocket;
      vsocket.m_btree = btree;
      vsocket.m_id = vtree.m_sockets_by_id.size();
      RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &vsocket.m_rna);

      vnode.m_inputs.append(&vsocket);
      vtree.m_input_sockets.append(&vsocket);
      vtree.m_sockets_by_id.append(&vsocket);
    }

    for (bNodeSocket *bsocket : BSocketList(bnode->outputs)) {
      VOutputSocket &vsocket = *vtree.m_allocator.construct<VOutputSocket>().release();

      vsocket.m_node = &vnode;
      vsocket.m_index = vnode.m_outputs.size();
      vsocket.m_is_input = false;
      vsocket.m_bsocket = bsocket;
      vsocket.m_btree = btree;
      vsocket.m_id = vtree.m_sockets_by_id.size();
      RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &vsocket.m_rna);

      vnode.m_outputs.append(&vsocket);
      vtree.m_output_sockets.append(&vsocket);
      vtree.m_sockets_by_id.append(&vsocket);
    }

    vtree.m_nodes_by_id.append(&vnode);
    node_mapping.add_new(bnode, &vnode);
  }

  for (bNodeLink *blink : BLinkList(btree->links)) {
    VOutputSocket &from_vsocket =
        *node_mapping.lookup(blink->fromnode)
             ->m_outputs[BSocketList(blink->fromnode->outputs).index_of(blink->fromsock)];
    VInputSocket &to_vsocket =
        *node_mapping.lookup(blink->tonode)
             ->m_inputs[BSocketList(blink->tonode->inputs).index_of(blink->tosock)];

    from_vsocket.m_directly_linked_sockets.append(&to_vsocket);
    to_vsocket.m_directly_linked_sockets.append(&from_vsocket);
  }

  for (VOutputSocket *socket : vtree.m_output_sockets) {
    if (!is_reroute_node(socket->node())) {
      vtree.find_targets_skipping_reroutes(*socket, socket->m_linked_sockets);
      for (VSocket *target : socket->m_linked_sockets) {
        target->m_linked_sockets.append(socket);
      }
    }
  }

  for (VNode *vnode : vtree.m_nodes_by_id) {
    if (vtree.m_nodes_by_idname.contains(vnode->idname())) {
      vtree.m_nodes_by_idname.lookup(vnode->idname()).append(vnode);
    }
    else {
      vtree.m_nodes_by_idname.add_new(vnode->idname(), {vnode});
    }
  }

  return std::unique_ptr<VirtualNodeTree>(vtree_ptr);
}

void VirtualNodeTree::find_targets_skipping_reroutes(VOutputSocket &vsocket,
                                                     Vector<VSocket *> &r_targets)
{
  for (VSocket *direct_target : vsocket.m_directly_linked_sockets) {
    if (is_reroute_node(*direct_target->m_node)) {
      this->find_targets_skipping_reroutes(*direct_target->m_node->m_outputs[0], r_targets);
    }
    else if (!r_targets.contains(direct_target)) {
      r_targets.append(direct_target);
    }
  }
}

VirtualNodeTree::~VirtualNodeTree()
{
  for (VNode *node : m_nodes_by_id) {
    node->~VNode();
  }
  for (VInputSocket *socket : m_input_sockets) {
    socket->~VInputSocket();
  }
  for (VOutputSocket *socket : m_output_sockets) {
    socket->~VOutputSocket();
  }
}

};  // namespace BKE
