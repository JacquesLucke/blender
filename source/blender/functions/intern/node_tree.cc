#include "FN_node_tree.h"

#include "BLI_string.h"
#include "BLI_dot_export.h"

extern "C" {
void WM_clipboard_text_set(const char *buf, bool selection);
}

namespace FN {

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
  if (a_index > b_index) {
    return false;
  }

  /* TODO: Match sorting with Python. */
  return BLI_strcasecmp(a->name().data(), b->name().data()) == -1;
}

static Vector<const VOutputSocket *> get_group_inputs(const VirtualNodeTree &vtree)
{
  Vector<const VNode *> input_vnodes = vtree.nodes_with_idname("fn_GroupInputNode");
  std::sort(input_vnodes.begin(), input_vnodes.end(), cmp_group_interface_nodes);

  Vector<const VOutputSocket *> input_vsockets;
  for (const VNode *vnode : input_vnodes) {
    input_vsockets.append(&vnode->output(0));
  }

  return input_vsockets;
}

static Vector<const VInputSocket *> get_group_outputs(const VirtualNodeTree &vtree)
{
  Vector<const VNode *> output_vnodes = vtree.nodes_with_idname("fn_GroupOutputNode");
  std::sort(output_vnodes.begin(), output_vnodes.end(), cmp_group_interface_nodes);

  Vector<const VInputSocket *> output_vsockets;
  for (const VNode *vnode : output_vnodes) {
    output_vsockets.append(&vnode->input(0));
  }

  return output_vsockets;
}

static bool is_input_interface_vnode(const VNode &vnode)
{
  return vnode.idname() == "fn_GroupInputNode";
}

static bool is_output_interface_vnode(const VNode &vnode)
{
  return vnode.idname() == "fn_GroupOutputNode";
}

static bool is_interface_node(const VNode &vnode)
{
  return is_input_interface_vnode(vnode) || is_output_interface_vnode(vnode);
}

static bool is_group_node(const VNode &vnode)
{
  return vnode.idname() == "fn_GroupNode";
}

FunctionNodeTree::~FunctionNodeTree()
{
  for (FNode *fnode : m_node_by_id) {
    fnode->~FNode();
  }
  for (FGroupInput *xgroup_input : m_group_inputs) {
    xgroup_input->~FGroupInput();
  }
  for (FParentNode *xparent_node : m_parent_nodes) {
    xparent_node->~FParentNode();
  }
  for (FInputSocket *fsocket : m_input_sockets) {
    fsocket->~FInputSocket();
  }
  for (FOutputSocket *fsocket : m_output_sockets) {
    fsocket->~FOutputSocket();
  }
}

void FNode::destruct_with_sockets()
{
  for (FInputSocket *socket : m_inputs) {
    socket->~FInputSocket();
  }
  for (FOutputSocket *socket : m_outputs) {
    socket->~FOutputSocket();
  }
  this->~FNode();
}

BLI_NOINLINE FunctionNodeTree::FunctionNodeTree(bNodeTree *btree, BTreeVTreeMap &vtrees)
    : m_btree(btree)
{
  const VirtualNodeTree &main_vtree = get_vtree(vtrees, btree);

  Vector<FNode *> all_nodes;
  Vector<FGroupInput *> all_group_inputs;
  Vector<FParentNode *> all_parent_nodes;

  this->insert_linked_nodes_for_vtree_in_id_order(main_vtree, all_nodes, nullptr);
  this->expand_groups(all_nodes, all_group_inputs, all_parent_nodes, vtrees);
  this->remove_expanded_groups_and_interfaces(all_nodes);
  this->store_tree_in_this_and_init_ids(
      std::move(all_nodes), std::move(all_group_inputs), std::move(all_parent_nodes));
}

BLI_NOINLINE void FunctionNodeTree::expand_groups(Vector<FNode *> &all_nodes,
                                                  Vector<FGroupInput *> &all_group_inputs,
                                                  Vector<FParentNode *> &all_parent_nodes,
                                                  BTreeVTreeMap &vtrees)
{
  for (uint i = 0; i < all_nodes.size(); i++) {
    FNode &current_node = *all_nodes[i];
    if (is_group_node(*current_node.m_vnode)) {
      this->expand_group_node(current_node, all_nodes, all_group_inputs, all_parent_nodes, vtrees);
    }
  }
}

BLI_NOINLINE void FunctionNodeTree::expand_group_node(FNode &group_node,
                                                      Vector<FNode *> &all_nodes,
                                                      Vector<FGroupInput *> &all_group_inputs,
                                                      Vector<FParentNode *> &all_parent_nodes,
                                                      BTreeVTreeMap &vtrees)
{
  BLI_assert(is_group_node(*group_node.m_vnode));
  const VNode &group_vnode = *group_node.m_vnode;
  bNodeTree *btree = (bNodeTree *)RNA_pointer_get(group_vnode.rna(), "node_group").data;
  if (btree == nullptr) {
    return;
  }

  const VirtualNodeTree &vtree = get_vtree(vtrees, btree);

  FParentNode &sub_parent = *m_allocator.construct<FParentNode>().release();
  sub_parent.m_id = all_parent_nodes.append_and_get_index(&sub_parent);
  sub_parent.m_parent = group_node.m_parent;
  sub_parent.m_vnode = &group_vnode;

  this->insert_linked_nodes_for_vtree_in_id_order(vtree, all_nodes, &sub_parent);
  ArrayRef<FNode *> new_fnodes_by_id = all_nodes.as_ref().take_back(vtree.nodes().size());

  this->expand_group__group_inputs_for_unlinked_inputs(group_node, all_group_inputs);
  this->expand_group__relink_inputs(vtree, new_fnodes_by_id, group_node);
  this->expand_group__relink_outputs(vtree, new_fnodes_by_id, group_node);
}

BLI_NOINLINE void FunctionNodeTree::expand_group__group_inputs_for_unlinked_inputs(
    FNode &group_node, Vector<FGroupInput *> &all_group_inputs)
{
  for (FInputSocket *input_socket : group_node.m_inputs) {
    if (!input_socket->is_linked()) {
      FGroupInput &group_input = *m_allocator.construct<FGroupInput>().release();
      group_input.m_id = all_group_inputs.append_and_get_index(&group_input);
      group_input.m_vsocket = &input_socket->m_vsocket->as_input();
      group_input.m_parent = group_node.m_parent;

      group_input.m_linked_sockets.append(input_socket);
      input_socket->m_linked_group_inputs.append(&group_input);
    }
  }
}

BLI_NOINLINE void FunctionNodeTree::expand_group__relink_inputs(const VirtualNodeTree &vtree,
                                                                ArrayRef<FNode *> new_fnodes_by_id,
                                                                FNode &group_node)
{
  Vector<const VOutputSocket *> group_inputs = get_group_inputs(vtree);

  for (uint input_index : group_inputs.index_iterator()) {
    const VOutputSocket *inside_interface_vsocket = group_inputs[input_index];
    const VNode &inside_interface_vnode = inside_interface_vsocket->node();
    FNode *inside_interface_fnode = new_fnodes_by_id[inside_interface_vnode.id()];

    FOutputSocket &inside_interface =
        *inside_interface_fnode->m_outputs[inside_interface_vsocket->index()];
    FInputSocket &outside_interface = *group_node.m_inputs[input_index];

    for (FOutputSocket *outside_connected : outside_interface.m_linked_sockets) {
      outside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&outside_interface);
    }

    for (FGroupInput *outside_connected : outside_interface.m_linked_group_inputs) {
      outside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&outside_interface);
    }

    for (FInputSocket *inside_connected : inside_interface.m_linked_sockets) {
      inside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&inside_interface);

      for (FOutputSocket *outside_connected : outside_interface.m_linked_sockets) {
        inside_connected->m_linked_sockets.append(outside_connected);
        outside_connected->m_linked_sockets.append(inside_connected);
      }

      for (FGroupInput *outside_connected : outside_interface.m_linked_group_inputs) {
        inside_connected->m_linked_group_inputs.append(outside_connected);
        outside_connected->m_linked_sockets.append(inside_connected);
      }
    }

    inside_interface.m_linked_sockets.clear();
    outside_interface.m_linked_sockets.clear();
    outside_interface.m_linked_group_inputs.clear();
  }
}

BLI_NOINLINE void FunctionNodeTree::expand_group__relink_outputs(
    const VirtualNodeTree &vtree, ArrayRef<FNode *> new_fnodes_by_id, FNode &group_node)
{
  Vector<const VInputSocket *> group_outputs = get_group_outputs(vtree);

  for (uint output_index : group_outputs.index_iterator()) {
    const VInputSocket *inside_interface_vsocket = group_outputs[output_index];
    const VNode &inside_interface_vnode = inside_interface_vsocket->node();
    FNode *inside_interface_fnode = new_fnodes_by_id[inside_interface_vnode.id()];

    FInputSocket &inside_interface =
        *inside_interface_fnode->m_inputs[inside_interface_vsocket->index()];
    FOutputSocket &outside_interface = *group_node.m_outputs[output_index];

    for (FOutputSocket *inside_connected : inside_interface.m_linked_sockets) {
      inside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&inside_interface);

      for (FInputSocket *outside_connected : outside_interface.m_linked_sockets) {
        inside_connected->m_linked_sockets.append(outside_connected);
        outside_connected->m_linked_sockets.append(inside_connected);
      }
    }

    for (FGroupInput *inside_connected : inside_interface.m_linked_group_inputs) {
      inside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&inside_interface);

      for (FInputSocket *outside_connected : outside_interface.m_linked_sockets) {
        inside_connected->m_linked_sockets.append(outside_connected);
        outside_connected->m_linked_group_inputs.append(inside_connected);
      }
    }

    for (FInputSocket *outside_connected : outside_interface.m_linked_sockets) {
      outside_connected->m_linked_sockets.remove_first_occurrence_and_reorder(&outside_interface);
    }

    outside_interface.m_linked_sockets.clear();
    inside_interface.m_linked_group_inputs.clear();
  }
}

BLI_NOINLINE void FunctionNodeTree::insert_linked_nodes_for_vtree_in_id_order(
    const VirtualNodeTree &vtree, Vector<FNode *> &all_nodes, FParentNode *parent)
{
  BLI::LargeScopedArray<FSocket *> sockets_map(vtree.socket_count());

  /* Insert nodes of group. */
  for (const VNode *vnode : vtree.nodes()) {
    FNode &node = this->create_node(*vnode, parent, sockets_map);
    all_nodes.append(&node);
  }

  /* Insert links of group. */
  for (const VNode *vnode : vtree.nodes()) {
    for (const VInputSocket *to_vsocket : vnode->inputs()) {
      FInputSocket *to_socket = (FInputSocket *)sockets_map[to_vsocket->id()];
      for (const VOutputSocket *from_vsocket : to_vsocket->linked_sockets()) {
        FOutputSocket *from_socket = (FOutputSocket *)sockets_map[from_vsocket->id()];
        to_socket->m_linked_sockets.append(from_socket);
        from_socket->m_linked_sockets.append(to_socket);
      }
    }
  }
}

BLI_NOINLINE FNode &FunctionNodeTree::create_node(const VNode &vnode,
                                                  FParentNode *parent,
                                                  MutableArrayRef<FSocket *> sockets_map)
{
  FNode &new_node = *m_allocator.construct<FNode>().release();
  new_node.m_vnode = &vnode;
  new_node.m_parent = parent;
  new_node.m_id = UINT32_MAX;

  for (const VInputSocket *vsocket : vnode.inputs()) {
    FInputSocket &new_socket = *m_allocator.construct<FInputSocket>().release();
    new_socket.m_vsocket = vsocket;
    new_socket.m_node = &new_node;
    new_socket.m_id = UINT32_MAX;
    new_socket.m_is_input = true;

    new_node.m_inputs.append_and_get_index(&new_socket);
    sockets_map[vsocket->id()] = &new_socket;
  }

  for (const VOutputSocket *vsocket : vnode.outputs()) {
    FOutputSocket &new_socket = *m_allocator.construct<FOutputSocket>().release();
    new_socket.m_vsocket = vsocket;
    new_socket.m_node = &new_node;
    new_socket.m_id = UINT32_MAX;
    new_socket.m_is_input = false;

    new_node.m_outputs.append_and_get_index(&new_socket);
    sockets_map[vsocket->id()] = &new_socket;
  }

  return new_node;
}

BLI_NOINLINE void FunctionNodeTree::remove_expanded_groups_and_interfaces(
    Vector<FNode *> &all_nodes)
{
  for (int i = 0; i < all_nodes.size(); i++) {
    FNode *current_node = all_nodes[i];
    if (is_group_node(current_node->vnode()) ||
        (is_interface_node(current_node->vnode()) && current_node->parent() != nullptr)) {
      all_nodes.remove_and_reorder(i);
      current_node->destruct_with_sockets();
      i--;
    }
  }
}

BLI_NOINLINE void FunctionNodeTree::store_tree_in_this_and_init_ids(
    Vector<FNode *> &&all_nodes,
    Vector<FGroupInput *> &&all_group_inputs,
    Vector<FParentNode *> &&all_parent_nodes)
{
  m_node_by_id = std::move(all_nodes);
  m_group_inputs = std::move(all_group_inputs);
  m_parent_nodes = std::move(all_parent_nodes);

  for (uint node_index : m_node_by_id.index_iterator()) {
    FNode *fnode = m_node_by_id[node_index];
    fnode->m_id = node_index;

    if (m_nodes_by_idname.contains(fnode->idname())) {
      m_nodes_by_idname.lookup(fnode->idname()).append(fnode);
    }
    else {
      m_nodes_by_idname.add_new(fnode->idname(), {fnode});
    }

    for (FInputSocket *fsocket : fnode->m_inputs) {
      fsocket->m_id = m_sockets_by_id.append_and_get_index(fsocket);
      m_input_sockets.append(fsocket);
    }
    for (FOutputSocket *fsocket : fnode->m_outputs) {
      fsocket->m_id = m_sockets_by_id.append_and_get_index(fsocket);
      m_output_sockets.append(fsocket);
    }
  }
}

static BLI::DotExport::Cluster *get_cluster_for_parent(
    BLI::DotExport::DirectedGraph &graph,
    Map<const FParentNode *, BLI::DotExport::Cluster *> &clusters,
    const FParentNode *parent)
{
  if (parent == nullptr) {
    return nullptr;
  }
  else if (!clusters.contains(parent)) {
    auto *parent_cluster = get_cluster_for_parent(graph, clusters, parent->parent());
    const VNode &parent_node = parent->vnode();
    bNodeTree *btree = (bNodeTree *)RNA_pointer_get(parent_node.rna(), "node_group").data;
    auto &new_cluster = graph.new_cluster(parent->vnode().name() + " / " +
                                          StringRef(btree->id.name + 2));
    new_cluster.set_parent_cluster(parent_cluster);
    clusters.add_new(parent, &new_cluster);
    return &new_cluster;
  }
  else {
    return clusters.lookup(parent);
  }
}

std::string FunctionNodeTree::to_dot() const
{
  BLI::DotExport::DirectedGraph digraph;
  digraph.set_rankdir(BLI::DotExport::Attr_rankdir::LeftToRight);

  Map<const FNode *, BLI::DotExport::Utils::NodeWithSocketsWrapper> dot_nodes;
  Map<const FGroupInput *, BLI::DotExport::Utils::NodeWithSocketsWrapper> dot_group_inputs;
  Map<const FParentNode *, BLI::DotExport::Cluster *> dot_clusters;

  for (const FNode *fnode : m_node_by_id) {
    auto &dot_node = digraph.new_node("");
    dot_node.set_attribute("bgcolor", "white");
    dot_node.set_attribute("style", "filled");

    Vector<std::string> input_names;
    for (const FInputSocket *input : fnode->inputs()) {
      input_names.append(input->m_vsocket->name());
    }
    Vector<std::string> output_names;
    for (const FOutputSocket *output : fnode->outputs()) {
      output_names.append(output->m_vsocket->name());
    }

    dot_nodes.add_new(fnode,
                      BLI::DotExport::Utils::NodeWithSocketsWrapper(
                          dot_node, fnode->vnode().name(), input_names, output_names));

    BLI::DotExport::Cluster *cluster = get_cluster_for_parent(
        digraph, dot_clusters, fnode->parent());
    dot_node.set_parent_cluster(cluster);

    for (const FInputSocket *input : fnode->inputs()) {
      for (const FGroupInput *group_input : input->linked_group_inputs()) {
        if (!dot_group_inputs.contains(group_input)) {
          auto &dot_group_input_node = digraph.new_node("");
          dot_group_input_node.set_attribute("bgcolor", "white");
          dot_group_input_node.set_attribute("style", "filled");

          std::string group_input_name = group_input->vsocket().name();

          dot_group_inputs.add_new(
              group_input,
              BLI::DotExport::Utils::NodeWithSocketsWrapper(
                  dot_group_input_node, "Group Input", {}, {group_input_name}));

          BLI::DotExport::Cluster *cluster = get_cluster_for_parent(
              digraph, dot_clusters, group_input->parent());
          dot_group_input_node.set_parent_cluster(cluster);
        }
      }
    }
  }

  for (const FNode *to_fnode : m_node_by_id) {
    auto to_dot_node = dot_nodes.lookup(to_fnode);

    for (const FInputSocket *to_fsocket : to_fnode->inputs()) {
      for (const FOutputSocket *from_fsocket : to_fsocket->linked_sockets()) {
        const FNode *from_fnode = &from_fsocket->node();

        auto from_dot_node = dot_nodes.lookup(from_fnode);

        digraph.new_edge(from_dot_node.output(from_fsocket->vsocket().index()),
                         to_dot_node.input(to_fsocket->vsocket().index()));
      }
      for (const FGroupInput *group_input : to_fsocket->linked_group_inputs()) {
        auto from_dot_node = dot_group_inputs.lookup(group_input);

        digraph.new_edge(from_dot_node.output(0),
                         to_dot_node.input(to_fsocket->vsocket().index()));
      }
    }
  }

  digraph.set_random_cluster_bgcolors();
  return digraph.to_dot_string();
}

void FunctionNodeTree::to_dot__clipboard() const
{
  std::string dot = this->to_dot();
  WM_clipboard_text_set(dot.c_str(), false);
}

const FInputSocket *FNode::input_with_name_prefix(StringRef name_prefix) const
{
  for (const FInputSocket *fsocket : m_inputs) {
    if (fsocket->name().startswith(name_prefix)) {
      return fsocket;
    }
  }
  return nullptr;
}

}  // namespace FN
