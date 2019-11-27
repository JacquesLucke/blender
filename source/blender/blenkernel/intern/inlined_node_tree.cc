#include "BKE_inlined_node_tree.h"

#include "BLI_string.h"
#include "BLI_dot_export.h"

extern "C" {
void WM_clipboard_text_set(const char *buf, bool selection);
}

namespace BKE {

static const VirtualNodeTree &get_vtree(BTreeVTreeMap &vtrees, bNodeTree *btree)
{
  return *vtrees.lookup_or_add(btree,
                               [btree]() { return BLI::make_unique<VirtualNodeTree>(btree); });
}

static bool cmp_group_interface_nodes(const VNode *a, const VNode *b)
{
  int a_index = RNA_int_get(a->rna(), "sort_index");
  int b_index = RNA_int_get(b->rna(), "sort_index");
  if (a_index < b_index) {
    return true;
  }

  /* TODO: Match sorting with Python. */
  return BLI_strcasecmp(a->name().data(), b->name().data()) == -1;
}

static Vector<const VOutputSocket *> get_group_inputs(const VirtualNodeTree &vtree)
{
  Vector<const VNode *> input_vnodes = vtree.nodes_with_idname("fn_GroupDataInputNode");
  std::sort(input_vnodes.begin(), input_vnodes.end(), cmp_group_interface_nodes);

  Vector<const VOutputSocket *> input_vsockets;
  for (const VNode *vnode : input_vnodes) {
    input_vsockets.append(&vnode->output(0));
  }

  return input_vsockets;
}

static Vector<const VInputSocket *> get_group_outputs(const VirtualNodeTree &vtree)
{
  Vector<const VNode *> output_vnodes = vtree.nodes_with_idname("fn_GroupDataOutputNode");
  std::sort(output_vnodes.begin(), output_vnodes.end(), cmp_group_interface_nodes);

  Vector<const VInputSocket *> output_vsockets;
  for (const VNode *vnode : output_vnodes) {
    output_vsockets.append(&vnode->input(0));
  }

  return output_vsockets;
}

static bool is_input_interface_vnode(const VNode &vnode)
{
  return vnode.idname() == "fn_GroupDataInputNode";
}

static bool is_output_interface_vnode(const VNode &vnode)
{
  return vnode.idname() == "fn_GroupDataOutputNode";
}

static bool is_interface_node(const VNode &vnode)
{
  return is_input_interface_vnode(vnode) || is_output_interface_vnode(vnode);
}

static bool is_group_node(const VNode &vnode)
{
  return vnode.idname() == "fn_GroupNode";
}

InlinedNodeTree::~InlinedNodeTree()
{
  for (XNode *xnode : m_node_by_id) {
    xnode->~XNode();
  }
  for (XGroupInput *xgroup_input : m_group_inputs) {
    xgroup_input->~XGroupInput();
  }
  for (XParentNode *xparent_node : m_parent_nodes) {
    xparent_node->~XParentNode();
  }
  for (XInputSocket *xsocket : m_input_sockets) {
    xsocket->~XInputSocket();
  }
  for (XOutputSocket *xsocket : m_output_sockets) {
    xsocket->~XOutputSocket();
  }
}

void XNode::destruct_with_sockets()
{
  for (XInputSocket *socket : m_inputs) {
    socket->~XInputSocket();
  }
  for (XOutputSocket *socket : m_outputs) {
    socket->~XOutputSocket();
  }
  this->~XNode();
}

BLI_NOINLINE InlinedNodeTree::InlinedNodeTree(bNodeTree *btree, BTreeVTreeMap &vtrees)
    : m_btree(btree)
{
  SCOPED_TIMER(__func__);
  const VirtualNodeTree &main_vtree = get_vtree(vtrees, btree);

  Vector<XNode *> all_nodes;
  Vector<XGroupInput *> all_group_inputs;
  Vector<XParentNode *> all_parent_nodes;

  this->insert_linked_nodes_for_vtree_in_id_order(main_vtree, all_nodes, nullptr);
  this->expand_groups(all_nodes, all_group_inputs, all_parent_nodes, vtrees);
  this->remove_expanded_groups_and_interfaces(all_nodes);
  this->store_tree_in_this_and_init_ids(
      std::move(all_nodes), std::move(all_group_inputs), std::move(all_parent_nodes));
}

BLI_NOINLINE void InlinedNodeTree::expand_groups(Vector<XNode *> &all_nodes,
                                                 Vector<XGroupInput *> &all_group_inputs,
                                                 Vector<XParentNode *> &all_parent_nodes,
                                                 BTreeVTreeMap &vtrees)
{
  for (uint i = 0; i < all_nodes.size(); i++) {
    XNode &current_node = *all_nodes[i];
    if (is_group_node(*current_node.m_vnode)) {
      this->expand_group_node(current_node, all_nodes, all_group_inputs, all_parent_nodes, vtrees);
    }
  }
}

BLI_NOINLINE void InlinedNodeTree::expand_group_node(XNode &group_node,
                                                     Vector<XNode *> &all_nodes,
                                                     Vector<XGroupInput *> &all_group_inputs,
                                                     Vector<XParentNode *> &all_parent_nodes,
                                                     BTreeVTreeMap &vtrees)
{
  BLI_assert(is_group_node(*group_node.m_vnode));
  const VNode &group_vnode = *group_node.m_vnode;
  bNodeTree *btree = (bNodeTree *)RNA_pointer_get(group_vnode.rna(), "node_group").data;
  if (btree == nullptr) {
    return;
  }

  const VirtualNodeTree &vtree = get_vtree(vtrees, btree);

  XParentNode &sub_parent = *m_allocator.construct<XParentNode>().release();
  sub_parent.m_id = all_parent_nodes.append_and_get_index(&sub_parent);
  sub_parent.m_parent = group_node.m_parent;
  sub_parent.m_vnode = &group_vnode;

  this->insert_linked_nodes_for_vtree_in_id_order(vtree, all_nodes, &sub_parent);
  ArrayRef<XNode *> new_xnodes_by_id = all_nodes.as_ref().take_back(vtree.nodes().size());
  this->expand_group__relink_inputs(vtree, new_xnodes_by_id, all_group_inputs, group_node);
  this->expand_group__relink_outputs(vtree, new_xnodes_by_id, group_node);
}

BLI_NOINLINE void InlinedNodeTree::expand_group__relink_inputs(
    const VirtualNodeTree &vtree,
    ArrayRef<XNode *> new_xnodes_by_id,
    Vector<XGroupInput *> &all_group_inputs,
    XNode &group_node)
{
  Vector<const VOutputSocket *> group_inputs = get_group_inputs(vtree);

  for (uint input_index : group_inputs.index_iterator()) {
    const VOutputSocket *inside_interface_vsocket = group_inputs[input_index];
    const VNode &inside_interface_vnode = inside_interface_vsocket->node();
    XNode *inside_interface_xnode = new_xnodes_by_id[inside_interface_vnode.id()];

    XOutputSocket &inside_interface =
        *inside_interface_xnode->m_outputs[inside_interface_vsocket->index()];
    XInputSocket &outside_interface = *group_node.m_inputs[input_index];

    /* Create group input for unconnected inputs. */
    if (outside_interface.is_linked()) {
      XGroupInput &group_input = *m_allocator.construct<XGroupInput>().release();
      group_input.m_id = all_group_inputs.append_and_get_index(&group_input);
      group_input.m_vsocket = &outside_interface.m_vsocket->as_input();
      group_input.m_parent = group_node.m_parent;

      group_input.m_linked_sockets.append(&outside_interface);
      outside_interface.m_linked_group_inputs.append(&group_input);
    }

    for (XOutputSocket *outside_connected : outside_interface.m_linked_sockets) {
      outside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&outside_interface);
    }

    for (XGroupInput *outside_connected : outside_interface.m_linked_group_inputs) {
      outside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&outside_interface);
    }

    for (XInputSocket *inside_connected : inside_interface.m_linked_sockets) {
      inside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&inside_interface);

      for (XOutputSocket *outside_connected : outside_interface.m_linked_sockets) {
        inside_connected->m_linked_sockets.append(outside_connected);
        outside_connected->m_linked_sockets.append(inside_connected);
      }

      for (XGroupInput *outside_connected : outside_interface.m_linked_group_inputs) {
        inside_connected->m_linked_group_inputs.append(outside_connected);
        outside_connected->m_linked_sockets.append(inside_connected);
      }
    }

    inside_interface.m_linked_sockets.clear();
    outside_interface.m_linked_sockets.clear();
    outside_interface.m_linked_group_inputs.clear();
  }
}

BLI_NOINLINE void InlinedNodeTree::expand_group__relink_outputs(const VirtualNodeTree &vtree,
                                                                ArrayRef<XNode *> new_xnodes_by_id,
                                                                XNode &group_node)
{
  Vector<const VInputSocket *> group_outputs = get_group_outputs(vtree);

  for (uint output_index : group_outputs.index_iterator()) {
    const VInputSocket *inside_interface_vsocket = group_outputs[output_index];
    const VNode &inside_interface_vnode = inside_interface_vsocket->node();
    XNode *inside_interface_xnode = new_xnodes_by_id[inside_interface_vnode.id()];

    XInputSocket &inside_interface =
        *inside_interface_xnode->m_inputs[inside_interface_vsocket->index()];
    XOutputSocket &outside_interface = *group_node.m_outputs[output_index];

    for (XOutputSocket *inside_connected : inside_interface.m_linked_sockets) {
      inside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&inside_interface);

      for (XInputSocket *outside_connected : outside_interface.m_linked_sockets) {
        inside_connected->m_linked_sockets.append(outside_connected);
        outside_connected->m_linked_sockets.append(inside_connected);
      }
    }

    for (XGroupInput *inside_connected : inside_interface.m_linked_group_inputs) {
      inside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&inside_interface);

      for (XInputSocket *outside_connected : outside_interface.m_linked_sockets) {
        inside_connected->m_linked_sockets.append(outside_connected);
        outside_connected->m_linked_group_inputs.append(inside_connected);
      }
    }

    for (XInputSocket *outside_connected : outside_interface.m_linked_sockets) {
      outside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&outside_interface);
    }

    outside_interface.m_linked_sockets.clear();
    inside_interface.m_linked_group_inputs.clear();
  }
}

BLI_NOINLINE void InlinedNodeTree::insert_linked_nodes_for_vtree_in_id_order(
    const VirtualNodeTree &vtree, Vector<XNode *> &all_nodes, XParentNode *parent)
{
  BLI::TemporaryArray<XSocket *> sockets_map(vtree.socket_count());

  /* Insert nodes of group. */
  for (const VNode *vnode : vtree.nodes()) {
    XNode &node = this->create_node(*vnode, parent, sockets_map);
    all_nodes.append(&node);
  }

  /* Insert links of group. */
  for (const VNode *vnode : vtree.nodes()) {
    for (const VInputSocket *to_vsocket : vnode->inputs()) {
      XInputSocket *to_socket = (XInputSocket *)sockets_map[to_vsocket->id()];
      for (const VOutputSocket *from_vsocket : to_vsocket->linked_sockets()) {
        XOutputSocket *from_socket = (XOutputSocket *)sockets_map[from_vsocket->id()];
        to_socket->m_linked_sockets.append(from_socket);
        from_socket->m_linked_sockets.append(to_socket);
      }
    }
  }
}

BLI_NOINLINE XNode &InlinedNodeTree::create_node(const VNode &vnode,
                                                 XParentNode *parent,
                                                 MutableArrayRef<XSocket *> sockets_map)
{
  XNode &new_node = *m_allocator.construct<XNode>().release();
  new_node.m_vnode = &vnode;
  new_node.m_parent = parent;
  new_node.m_id = UINT32_MAX;

  for (const VInputSocket *vsocket : vnode.inputs()) {
    XInputSocket &new_socket = *m_allocator.construct<XInputSocket>().release();
    new_socket.m_vsocket = vsocket;
    new_socket.m_node = &new_node;
    new_socket.m_id = UINT32_MAX;
    new_socket.m_is_input = true;

    new_node.m_inputs.append_and_get_index(&new_socket);
    sockets_map[vsocket->id()] = &new_socket;
  }

  for (const VOutputSocket *vsocket : vnode.outputs()) {
    XOutputSocket &new_socket = *m_allocator.construct<XOutputSocket>().release();
    new_socket.m_vsocket = vsocket;
    new_socket.m_node = &new_node;
    new_socket.m_id = UINT32_MAX;
    new_socket.m_is_input = false;

    new_node.m_outputs.append_and_get_index(&new_socket);
    sockets_map[vsocket->id()] = &new_socket;
  }

  return new_node;
}

BLI_NOINLINE void InlinedNodeTree::remove_expanded_groups_and_interfaces(
    Vector<XNode *> &all_nodes)
{
  for (int i = 0; i < all_nodes.size(); i++) {
    XNode *current_node = all_nodes[i];
    if (is_group_node(current_node->vnode()) ||
        (is_interface_node(current_node->vnode()) && current_node->parent() != nullptr)) {
      all_nodes.remove_and_reorder(i);
      current_node->destruct_with_sockets();
      i--;
    }
  }
}

BLI_NOINLINE void InlinedNodeTree::store_tree_in_this_and_init_ids(
    Vector<XNode *> &&all_nodes,
    Vector<XGroupInput *> &&all_group_inputs,
    Vector<XParentNode *> &&all_parent_nodes)
{
  m_node_by_id = std::move(all_nodes);
  m_group_inputs = std::move(all_group_inputs);
  m_parent_nodes = std::move(all_parent_nodes);

  for (uint node_index : m_node_by_id.index_iterator()) {
    XNode *xnode = m_node_by_id[node_index];
    xnode->m_id = node_index;

    if (m_nodes_by_idname.contains(xnode->idname())) {
      m_nodes_by_idname.lookup(xnode->idname()).append(xnode);
    }
    else {
      m_nodes_by_idname.add_new(xnode->idname(), {xnode});
    }

    for (XInputSocket *xsocket : xnode->m_inputs) {
      xsocket->m_id = m_sockets_by_id.append_and_get_index(xsocket);
      m_input_sockets.append(xsocket);
    }
    for (XOutputSocket *xsocket : xnode->m_outputs) {
      xsocket->m_id = m_sockets_by_id.append_and_get_index(xsocket);
      m_output_sockets.append(xsocket);
    }
  }
}

static BLI::DotExport::Cluster *get_cluster_for_parent(
    BLI::DotExport::DirectedGraph &graph,
    Map<const XParentNode *, BLI::DotExport::Cluster *> &clusters,
    const XParentNode *parent)
{
  if (parent == nullptr) {
    return nullptr;
  }
  else if (!clusters.contains(parent)) {
    auto *parent_cluster = get_cluster_for_parent(graph, clusters, parent->parent());
    auto &new_cluster = graph.new_cluster(parent->vnode().name());
    new_cluster.set_parent_cluster(parent_cluster);
    clusters.add_new(parent, &new_cluster);
    return &new_cluster;
  }
  else {
    return clusters.lookup(parent);
  }
}

std::string InlinedNodeTree::to_dot() const
{
  BLI::DotExport::DirectedGraph digraph;
  digraph.set_rankdir(BLI::DotExport::Attr_rankdir::LeftToRight);

  Map<const XNode *, BLI::DotExport::Utils::NodeWithSocketsWrapper> dot_nodes;
  Map<const XGroupInput *, BLI::DotExport::Utils::NodeWithSocketsWrapper> dot_group_inputs;
  Map<const XParentNode *, BLI::DotExport::Cluster *> dot_clusters;

  for (const XNode *xnode : m_node_by_id) {
    auto &dot_node = digraph.new_node("");
    dot_node.set_attribute("bgcolor", "white");
    dot_node.set_attribute("style", "filled");

    Vector<std::string> input_names;
    for (const XInputSocket *input : xnode->inputs()) {
      input_names.append(input->m_vsocket->name());
    }
    Vector<std::string> output_names;
    for (const XOutputSocket *output : xnode->outputs()) {
      output_names.append(output->m_vsocket->name());
    }

    dot_nodes.add_new(xnode,
                      BLI::DotExport::Utils::NodeWithSocketsWrapper(
                          dot_node, xnode->vnode().name(), input_names, output_names));

    BLI::DotExport::Cluster *cluster = get_cluster_for_parent(
        digraph, dot_clusters, xnode->parent());
    dot_node.set_parent_cluster(cluster);

    for (const XInputSocket *input : xnode->inputs()) {
      for (const XGroupInput *group_input : input->linked_group_inputs()) {
        if (!dot_group_inputs.contains(group_input)) {
          auto &dot_group_input_node = digraph.new_node("");
          dot_group_input_node.set_attribute("bgcolor", "white");
          dot_group_input_node.set_attribute("style", "filled");

          dot_group_inputs.add_new(group_input,
                                   BLI::DotExport::Utils::NodeWithSocketsWrapper(
                                       dot_group_input_node, "Group Input", {}, {"Value"}));

          BLI::DotExport::Cluster *cluster = get_cluster_for_parent(
              digraph, dot_clusters, group_input->parent());
          dot_group_input_node.set_parent_cluster(cluster);
        }
      }
    }
  }

  for (const XNode *to_xnode : m_node_by_id) {
    auto to_dot_node = dot_nodes.lookup(to_xnode);

    for (const XInputSocket *to_xsocket : to_xnode->inputs()) {
      for (const XOutputSocket *from_xsocket : to_xsocket->linked_sockets()) {
        const XNode *from_xnode = &from_xsocket->node();

        auto from_dot_node = dot_nodes.lookup(from_xnode);

        digraph.new_edge(from_dot_node.output(from_xsocket->vsocket().index()),
                         to_dot_node.input(to_xsocket->vsocket().index()));
      }
      for (const XGroupInput *group_input : to_xsocket->linked_group_inputs()) {
        auto from_dot_node = dot_group_inputs.lookup(group_input);

        digraph.new_edge(from_dot_node.output(0),
                         to_dot_node.input(to_xsocket->vsocket().index()));
      }
    }
  }

  digraph.set_random_cluster_bgcolors();
  return digraph.to_dot_string();
}

void InlinedNodeTree::to_dot__clipboard() const
{
  std::string dot = this->to_dot();
  WM_clipboard_text_set(dot.c_str(), false);
}

}  // namespace BKE
