#include "BKE_virtual_node_tree_cxx.h"
#include "BLI_timeit.h"

namespace BKE {

void VirtualNodeTree::add_all_of_tree(bNodeTree *btree)
{
  Map<bNode *, VirtualNode *> node_mapping;
  for (bNode *bnode : bNodeList(btree->nodes)) {
    VirtualNode *vnode = this->add_bnode(btree, bnode);
    node_mapping.add_new(bnode, vnode);
  }
  for (bNodeLink *blink : bLinkList(btree->links)) {
    VirtualNode *from_vnode = node_mapping.lookup(blink->fromnode);
    VirtualNode *to_vnode = node_mapping.lookup(blink->tonode);
    VirtualSocket *from_vsocket = nullptr;
    VirtualSocket *to_vsocket = nullptr;

    for (VirtualSocket *output : from_vnode->outputs()) {
      if (output->bsocket() == blink->fromsock) {
        from_vsocket = output;
        break;
      }
    }

    for (VirtualSocket *input : to_vnode->inputs()) {
      if (input->bsocket() == blink->tosock) {
        to_vsocket = input;
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

  Vector<bNodeSocket *, 10> original_inputs(bnode->inputs, true);
  Vector<bNodeSocket *, 10> original_outputs(bnode->outputs, true);

  auto inputs = m_allocator.allocate_array<VirtualSocket>(original_inputs.size());
  auto outputs = m_allocator.allocate_array<VirtualSocket>(original_outputs.size());

  vnode->m_inputs = m_allocator.allocate_array<VirtualSocket *>(inputs.size());
  vnode->m_outputs = m_allocator.allocate_array<VirtualSocket *>(outputs.size());

  for (uint i = 0; i < original_inputs.size(); i++) {
    VirtualSocket &vsocket = inputs[i];
    new (&vsocket) VirtualSocket();
    vsocket.m_vnode = vnode;
    vsocket.m_btree = btree;
    vsocket.m_bsocket = original_inputs[i];
    vsocket.m_id = m_socket_counter++;
    vnode->m_inputs[i] = &vsocket;
  }
  for (uint i = 0; i < original_outputs.size(); i++) {
    VirtualSocket &vsocket = outputs[i];
    new (&vsocket) VirtualSocket();
    vsocket.m_vnode = vnode;
    vsocket.m_btree = btree;
    vsocket.m_bsocket = original_outputs[i];
    vsocket.m_id = m_socket_counter++;
    vnode->m_outputs[i] = &vsocket;
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
  MultiMap<VirtualSocket *, VirtualSocket *> connections;
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

static void find_connected_sockets_left(VirtualSocket *vsocket, Vector<VirtualSocket *> &r_found)
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

static void find_connected_sockets_right(VirtualSocket *vsocket, Vector<VirtualSocket *> &r_found)
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
      Vector<VirtualSocket *> found;
      find_connected_sockets_right(vsocket, found);
      vsocket->m_links = m_allocator.allocate_array<VirtualSocket *>(found.size());
      vsocket->m_links.copy_from(found);
    }
    if (vlink->m_to->m_links.size() == 0) {
      VirtualSocket *vsocket = vlink->m_to;
      Vector<VirtualSocket *> found;
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
