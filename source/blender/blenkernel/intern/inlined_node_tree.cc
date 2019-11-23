#include "BKE_inlined_node_tree.h"

#include "BLI_string.h"

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

InlinedNodeTree::InlinedNodeTree(bNodeTree *btree, BTreeVTreeMap &vtrees) : m_btree(btree)
{
  const VirtualNodeTree &main_vtree = get_vtree(vtrees, btree);

  Vector<XNode *> nodes;

  Map<const VInputSocket *, XInputSocket *> inputs_map;
  Map<const VOutputSocket *, XOutputSocket *> outputs_map;

  /* Insert main nodes. */
  for (const VNode *vnode : main_vtree.nodes()) {
    XNode &node = this->create_node(*vnode, nullptr, inputs_map, outputs_map);
    nodes.append(&node);
  }

  /* Insert main links. */
  for (const VNode *vnode : main_vtree.nodes()) {
    for (const VInputSocket *to_vsocket : vnode->inputs()) {
      XInputSocket *to_socket = inputs_map.lookup(to_vsocket);
      for (const VOutputSocket *from_vsocket : to_vsocket->linked_sockets()) {
        XOutputSocket *from_socket = outputs_map.lookup(from_vsocket);
        to_socket->m_linked_sockets.append(from_socket);
        from_socket->m_linked_sockets.append(to_socket);
      }
    }
  }

  /* Expand node groups one after another. */
  for (uint i = 0; i < nodes.size(); i++) {
    XNode &current_node = *nodes[i];
    if (is_group_node(*current_node.m_vnode)) {
      this->expand_group_node(current_node, nodes, vtrees);
    }
  }

  /* Remove unused nodes. */
  for (int i = 0; i < nodes.size(); i++) {
    XNode &current_node = *nodes[i];
    if (is_group_node(*current_node.m_vnode)) {
      nodes.remove_and_reorder(i);
      i--;
    }
    else if (is_interface_node(*current_node.m_vnode) && current_node.m_parent != nullptr) {
      nodes.remove_and_reorder(i);
      i--;
    }
  }

  m_node_by_id = nodes;
}

void InlinedNodeTree::expand_group_node(XNode &group_node,
                                        Vector<XNode *> &nodes,
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
  sub_parent.m_parent = group_node.m_parent;
  sub_parent.m_vnode = &group_vnode;

  Map<const VInputSocket *, XInputSocket *> inputs_map;
  Map<const VOutputSocket *, XOutputSocket *> outputs_map;

  /* Insert nodes of group. */
  for (const VNode *vnode : vtree.nodes()) {
    XNode &node = this->create_node(*vnode, &sub_parent, inputs_map, outputs_map);
    nodes.append(&node);
  }

  /* Insert links of group. */
  for (const VNode *vnode : vtree.nodes()) {
    for (const VInputSocket *to_vsocket : vnode->inputs()) {
      XInputSocket *to_socket = inputs_map.lookup(to_vsocket);
      for (const VOutputSocket *from_vsocket : to_vsocket->linked_sockets()) {
        XOutputSocket *from_socket = outputs_map.lookup(from_vsocket);
        to_socket->m_linked_sockets.append(from_socket);
        from_socket->m_linked_sockets.append(to_socket);
      }
    }
  }

  Vector<const VOutputSocket *> group_inputs = get_group_inputs(vtree);
  Vector<const VInputSocket *> group_outputs = get_group_outputs(vtree);

  /* Relink links to group inputs. */
  for (const VOutputSocket *from_vsocket : group_inputs) {
    XOutputSocket &from_socket = *outputs_map.lookup(from_vsocket);

    /* If the group input has no origin, insert a dummy group input. */
    XInputSocket &outside_group_input = *group_node.m_inputs[from_vsocket->index()];
    if (outside_group_input.m_linked_sockets.size() == 0 &&
        outside_group_input.m_linked_group_inputs.size() == 0) {
      XGroupInput &group_input_dummy = *m_allocator.construct<XGroupInput>().release();
      group_input_dummy.m_vsocket = outside_group_input.m_vsocket;
      group_input_dummy.m_parent = group_node.m_parent;

      group_input_dummy.m_linked_sockets.append(&outside_group_input);
      outside_group_input.m_linked_group_inputs.append(&group_input_dummy);
    }

    for (XInputSocket *to_socket : from_socket.m_linked_sockets) {
      to_socket->m_linked_sockets.remove_first_occurrence_and_reorder(&from_socket);

      for (XOutputSocket *outer_from : outside_group_input.m_linked_sockets) {
        to_socket->m_linked_sockets.append(outer_from);
      }

      for (XGroupInput *outer_from : outside_group_input.m_linked_group_inputs) {
        to_socket->m_linked_group_inputs.append(outer_from);
      }
    }

    from_socket.m_linked_sockets.clear();
  }

  /* Relink links to group outputs. */
  for (const VInputSocket *to_vsocket : group_outputs) {
    XInputSocket &to_socket = *inputs_map.lookup(to_vsocket);
    XOutputSocket &outside_group_output = *group_node.m_outputs[to_vsocket->index()];

    for (XOutputSocket *from_socket : to_socket.m_linked_sockets) {
      from_socket->m_linked_sockets.remove_first_occurrence_and_reorder(&to_socket);

      for (XInputSocket *to_socket : outside_group_output.m_linked_sockets) {
        from_socket->m_linked_sockets.append(to_socket);
        to_socket->m_linked_sockets.append(from_socket);
      }
    }

    for (XGroupInput *from_socket : to_socket.m_linked_group_inputs) {
      from_socket->m_linked_sockets.remove_first_occurrence_and_reorder(&to_socket);

      for (XInputSocket *to_socket : outside_group_output.m_linked_sockets) {
        from_socket->m_linked_sockets.append(to_socket);
        to_socket->m_linked_group_inputs.append(from_socket);
      }
    }

    to_socket.m_linked_group_inputs.clear();
  }
}

XNode &InlinedNodeTree::create_node(const VNode &vnode,
                                    XParentNode *parent,
                                    Map<const VInputSocket *, XInputSocket *> &inputs_map,
                                    Map<const VOutputSocket *, XOutputSocket *> &outputs_map)
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

    new_node.m_inputs.append_and_get_index(&new_socket);
    inputs_map.add_new(vsocket, &new_socket);
  }

  for (const VOutputSocket *vsocket : vnode.outputs()) {
    XOutputSocket &new_socket = *m_allocator.construct<XOutputSocket>().release();
    new_socket.m_vsocket = vsocket;
    new_socket.m_node = &new_node;
    new_socket.m_id = UINT32_MAX;

    new_node.m_outputs.append_and_get_index(&new_socket);
    outputs_map.add_new(vsocket, &new_socket);
  }

  return new_node;
}

std::string InlinedNodeTree::to_dot() const
{
  /* TODO */
  return "";
}

void InlinedNodeTree::to_dot__clipboard() const
{
  std::string dot = this->to_dot();
  WM_clipboard_text_set(dot.c_str(), false);
}

}  // namespace BKE
