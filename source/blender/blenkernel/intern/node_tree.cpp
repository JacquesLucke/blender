#include "BKE_node_tree.hpp"
#include "BLI_timeit.hpp"

namespace BKE {

IndexedNodeTree::IndexedNodeTree(bNodeTree *btree)
    : m_btree(btree), m_original_nodes(btree->nodes, true), m_original_links(btree->links, true)
{
  for (bNode *bnode : m_original_nodes) {
    for (bNodeSocket *bsocket : bSocketList(bnode->inputs)) {
      m_node_by_socket.add(bsocket, bnode);
    }
    for (bNodeSocket *bsocket : bSocketList(bnode->outputs)) {
      m_node_by_socket.add(bsocket, bnode);
    }
  }

  for (bNode *bnode : m_original_nodes) {
    m_nodes_by_idname.add(bnode->idname, bnode);
    if (!this->is_reroute(bnode) && !this->is_frame(bnode)) {
      m_actual_nodes.append(bnode);
    }
  }

  for (bNodeLink *blink : m_original_links) {
    m_direct_links.add(blink->tosock, {blink->fromsock, blink->fromnode});
    m_direct_links.add(blink->fromsock, {blink->tosock, blink->tonode});
  }

  for (bNodeLink *blink : m_original_links) {
    if (!this->is_reroute(blink->fromnode) && !m_links.contains(blink->fromsock)) {
      SmallVector<SocketWithNode> connected;
      this->find_connected_sockets_right(blink->fromsock, connected);
      m_links.add_multiple_new(blink->fromsock, connected);
    }
    if (!this->is_reroute(blink->tonode) && !m_links.contains(blink->tosock)) {
      SmallVector<SocketWithNode> connected;
      this->find_connected_sockets_left(blink->tosock, connected);
      m_links.add_multiple_new(blink->tosock, connected);
      if (connected.size() == 1) {
        m_single_origin_links.append(SingleOriginLink{connected[0].socket, blink->tosock, blink});
      }
    }
  }
}

void IndexedNodeTree::find_connected_sockets_left(bNodeSocket *bsocket,
                                                  SmallVector<SocketWithNode> &r_sockets) const
{
  BLI_assert(bsocket->in_out == SOCK_IN);
  auto from_sockets = m_direct_links.lookup_default(bsocket);
  for (SocketWithNode linked : from_sockets) {
    if (this->is_reroute(linked.node)) {
      this->find_connected_sockets_left((bNodeSocket *)linked.node->inputs.first, r_sockets);
    }
    else {
      r_sockets.append(linked);
    }
  }
}
void IndexedNodeTree::find_connected_sockets_right(bNodeSocket *bsocket,
                                                   SmallVector<SocketWithNode> &r_sockets) const
{
  BLI_assert(bsocket->in_out == SOCK_OUT);
  auto to_sockets = m_direct_links.lookup_default(bsocket);
  for (SocketWithNode other : to_sockets) {
    if (this->is_reroute(other.node)) {
      this->find_connected_sockets_right((bNodeSocket *)other.node->outputs.first, r_sockets);
    }
    else {
      r_sockets.append(other);
    }
  }
}

bool IndexedNodeTree::is_reroute(bNode *bnode) const
{
  return STREQ(bnode->idname, "NodeReroute");
}

bool IndexedNodeTree::is_frame(bNode *bnode) const
{
  return STREQ(bnode->idname, "NodeFrame");
}

/* Queries
 *******************************************************/

ArrayRef<bNode *> IndexedNodeTree::nodes_with_idname(StringRef idname) const
{
  return m_nodes_by_idname.lookup_default(idname.to_std_string());
}

ArrayRef<SocketWithNode> IndexedNodeTree::linked(bNodeSocket *bsocket) const
{
  return m_links.lookup_default(bsocket);
}

ArrayRef<SingleOriginLink> IndexedNodeTree::single_origin_links() const
{
  return m_single_origin_links;
}

/* Virtual Node Tree
 *****************************************/

void VirtualNodeTree::add_all_of_tree(bNodeTree *btree)
{
  SmallMap<bNode *, VirtualNode *> node_mapping;
  for (bNode *bnode : BKE::bNodeList(btree->nodes)) {
    VirtualNode *vnode = this->add_bnode(btree, bnode);
    node_mapping.add_new(bnode, vnode);
  }
  for (bNodeLink *blink : BKE::bLinkList(btree->links)) {
    VirtualNode *from_node = node_mapping.lookup(blink->fromnode);
    VirtualNode *to_node = node_mapping.lookup(blink->tonode);
    VirtualSocket *from_vsocket = nullptr;
    VirtualSocket *to_vsocket = nullptr;

    for (VirtualSocket &output : from_node->outputs()) {
      if (output.bsocket() == blink->fromsock) {
        from_vsocket = &output;
        break;
      }
    }

    for (VirtualSocket &input : to_node->inputs()) {
      if (input.bsocket() == blink->tosock) {
        to_vsocket = &input;
        break;
      }
    }

    BLI_assert(from_vsocket);
    BLI_assert(to_vsocket);
    this->add_link(from_vsocket, to_vsocket);
  }
}

VirtualNode *VirtualNodeTree::add_bnode(bNodeTree *btree, bNode *bnode)
{
  BLI_assert(!m_frozen);

  VirtualNode *vnode = m_allocator.allocate<VirtualNode>();
  vnode->m_backlink = this;
  vnode->m_bnode = bnode;
  vnode->m_btree = btree;

  SmallVector<bNodeSocket *, 10> original_inputs(bnode->inputs, true);
  SmallVector<bNodeSocket *, 10> original_outputs(bnode->outputs, true);

  vnode->m_inputs = m_allocator.allocate_array<VirtualSocket>(original_inputs.size());
  vnode->m_outputs = m_allocator.allocate_array<VirtualSocket>(original_outputs.size());

  for (uint i = 0; i < original_inputs.size(); i++) {
    VirtualSocket &vsocket = vnode->m_inputs[i];
    new (&vsocket) VirtualSocket();
    vsocket.m_vnode = vnode;
    vsocket.m_btree = btree;
    vsocket.m_bsocket = original_inputs[i];
  }
  for (uint i = 0; i < original_outputs.size(); i++) {
    VirtualSocket &vsocket = vnode->m_outputs[i];
    new (&vsocket) VirtualSocket();
    vsocket.m_vnode = vnode;
    vsocket.m_btree = btree;
    vsocket.m_bsocket = original_outputs[i];
  }

  m_nodes.append(vnode);
  return vnode;
}

void VirtualNodeTree::add_link(VirtualSocket *a, VirtualSocket *b)
{
  BLI_assert(!m_frozen);

  VirtualLink *vlink = m_allocator.allocate<VirtualLink>();
  if (a->is_input()) {
    BLI_assert(b->is_output());
    vlink->m_from = b;
    vlink->m_to = a;
  }
  else {
    BLI_assert(b->is_input());
    vlink->m_from = a;
    vlink->m_to = b;
  }

  m_links.append(vlink);
}

void VirtualNodeTree::freeze_and_index()
{
  m_frozen = true;
  this->initialize_direct_links();
  this->initialize_links();
  this->initialize_nodes_by_idname();
}

BLI_NOINLINE void VirtualNodeTree::initialize_direct_links()
{
  /* TODO(jacques): reserve */
  SmallMultiMap<VirtualSocket *, VirtualSocket *> connections;
  for (VirtualLink *link : m_links) {
    connections.add(link->m_from, link->m_to);
    connections.add(link->m_to, link->m_from);
  }

  /* TODO(jacques): items iterator */
  for (VirtualSocket *vsocket : connections.keys()) {
    auto others = connections.lookup(vsocket);
    vsocket->m_direct_links = m_allocator.allocate_array<VirtualSocket *>(others.size());
    vsocket->m_direct_links.copy_from(others);
  }
}

static bool is_reroute(VirtualNode *vnode)
{
  return STREQ(vnode->bnode()->idname, "NodeReroute");
}

static void find_connected_sockets_left(VirtualSocket *vsocket,
                                        SmallVector<VirtualSocket *> &r_found)
{
  BLI_assert(vsocket->is_input());
  for (VirtualSocket *other : vsocket->direct_links()) {
    if (is_reroute(other->vnode())) {
      find_connected_sockets_left(other->vnode()->input(0), r_found);
    }
    else {
      r_found.append(other);
    }
  }
}

static void find_connected_sockets_right(VirtualSocket *vsocket,
                                         SmallVector<VirtualSocket *> &r_found)
{
  BLI_assert(vsocket->is_output());
  for (VirtualSocket *other : vsocket->direct_links()) {
    if (is_reroute(other->vnode())) {
      find_connected_sockets_right(other->vnode()->output(0), r_found);
    }
    else {
      r_found.append(other);
    }
  }
}

BLI_NOINLINE void VirtualNodeTree::initialize_links()
{
  for (VirtualLink *vlink : m_links) {
    if (vlink->m_from->m_links.size() == 0) {
      VirtualSocket *vsocket = vlink->m_from;
      SmallVector<VirtualSocket *> found;
      find_connected_sockets_right(vsocket, found);
      vsocket->m_links = m_allocator.allocate_array<VirtualSocket *>(found.size());
      vsocket->m_links.copy_from(found);
    }
    if (vlink->m_to->m_links.size() == 0) {
      VirtualSocket *vsocket = vlink->m_to;
      SmallVector<VirtualSocket *> found;
      find_connected_sockets_left(vsocket, found);
      vsocket->m_links = m_allocator.allocate_array<VirtualSocket *>(found.size());
      vsocket->m_links.copy_from(found);

      if (vsocket->m_links.size() > 0) {
        m_inputs_with_links.append(vlink->m_to);
      }
    }
  }
}

BLI_NOINLINE void VirtualNodeTree::initialize_nodes_by_idname()
{
  for (VirtualNode *vnode : m_nodes) {
    bNode *bnode = vnode->bnode();
    m_nodes_by_idname.add(bnode->idname, vnode);
  }
}

}  // namespace BKE
