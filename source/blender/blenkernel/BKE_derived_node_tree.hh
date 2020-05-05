/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __BKE_DERIVED_NODE_TREE_HH__
#define __BKE_DERIVED_NODE_TREE_HH__

#include "BKE_node_tree_ref.hh"

namespace BKE {

class DSocket;
class DInputSocket;
class DOutputSocket;
class DNode;
class DParentNode;
class DGroupInput;
class DerivedNodeTree;

class DSocket : BLI::NonCopyable, BLI::NonMovable {
 protected:
  DNode *m_node;
  const SocketRef *m_socket_ref;
  uint m_id;

  friend DerivedNodeTree;

 public:
  const DNode &node() const;

  uint id() const;
  uint index() const;

  bool is_input() const;
  bool is_output() const;

  const DSocket &as_base() const;
  const DInputSocket &as_input() const;
  const DOutputSocket &as_output() const;

  PointerRNA *rna() const;
  StringRefNull idname() const;
  StringRefNull name() const;
};

class DInputSocket : public DSocket {
 private:
  Vector<DOutputSocket *> m_linked_sockets;
  Vector<DGroupInput *> m_linked_group_inputs;

  friend DerivedNodeTree;

 public:
  const InputSocketRef &socket_ref() const;

  ArrayRef<const DOutputSocket *> linked_sockets() const;
  ArrayRef<const DGroupInput *> linked_group_inputs() const;

  bool is_linked() const;
};

class DOutputSocket : public DSocket {
 private:
  Vector<DInputSocket *> m_linked_sockets;

  friend DerivedNodeTree;

 public:
  const OutputSocketRef &socket_ref() const;
  ArrayRef<const DInputSocket *> linked_sockets() const;
};

class DGroupInput : BLI::NonCopyable, BLI::NonMovable {
 private:
  const InputSocketRef *m_socket_ref;
  DParentNode *m_parent;
  Vector<DInputSocket *> m_linked_sockets;
  uint m_id;

  friend DerivedNodeTree;

 public:
  const InputSocketRef &socket_ref() const;
  const DParentNode *parent() const;
  ArrayRef<const DInputSocket *> linked_sockets() const;
  uint id() const;
  StringRefNull name() const;
};

class DNode : BLI::NonCopyable, BLI::NonMovable {
 private:
  const NodeRef *m_node_ref;
  DParentNode *m_parent;

  MutableArrayRef<DInputSocket *> m_inputs;
  MutableArrayRef<DOutputSocket *> m_outputs;

  uint m_id;

  friend DerivedNodeTree;

 public:
  const NodeRef &node_ref() const;
  const DParentNode *parent() const;

  ArrayRef<const DInputSocket *> inputs() const;
  ArrayRef<const DOutputSocket *> outputs() const;

  const DInputSocket &input(uint index) const;
  const DOutputSocket &output(uint index) const;

  uint id() const;

  PointerRNA *rna() const;
  StringRefNull idname() const;
  StringRefNull name() const;

 private:
  void destruct_with_sockets();
};

class DParentNode : BLI::NonCopyable, BLI::NonMovable {
 private:
  const NodeRef *m_node_ref;
  DParentNode *m_parent;
  uint m_id;

  friend DerivedNodeTree;

 public:
  const DParentNode *parent() const;
  const NodeRef &node_ref() const;
  uint id() const;
};

using NodeTreeRefMap = Map<bNodeTree *, std::unique_ptr<const NodeTreeRef>>;

class DerivedNodeTree : BLI::NonCopyable, BLI::NonMovable {
 private:
  LinearAllocator<> m_allocator;
  bNodeTree *m_btree;
  Vector<DNode *> m_nodes_by_id;
  Vector<DGroupInput *> m_group_inputs;
  Vector<DParentNode *> m_parent_nodes;

  Vector<DSocket *> m_sockets_by_id;
  Vector<DInputSocket *> m_input_sockets;
  Vector<DOutputSocket *> m_output_sockets;

  StringMap<Vector<DNode *>> m_nodes_by_idname;

 public:
  DerivedNodeTree(bNodeTree *btree, NodeTreeRefMap &node_tree_refs);
  ~DerivedNodeTree();

  ArrayRef<const DNode *> all_nodes() const;

  std::string to_dot() const;

 private:
  /* Utility functions used during construction. */
  void insert_nodes_and_links_in_id_order(const NodeTreeRef &tree_ref,
                                          DParentNode *parent,
                                          Vector<DNode *> &all_nodes);
  DNode &create_node(const NodeRef &node_ref,
                     DParentNode *parent,
                     MutableArrayRef<DSocket *> r_sockets_map);
  void expand_groups(Vector<DNode *> &all_nodes,
                     Vector<DGroupInput *> &all_group_inputs,
                     Vector<DParentNode *> &all_parent_nodes,
                     NodeTreeRefMap &node_tree_refs);
  void expand_group_node(DNode &group_node,
                         Vector<DNode *> &all_nodes,
                         Vector<DGroupInput *> &all_group_inputs,
                         Vector<DParentNode *> &all_parent_nodes,
                         NodeTreeRefMap &node_tree_refs);
  void create_group_inputs_for_unlinked_inputs(DNode &node,
                                               Vector<DGroupInput *> &all_group_inputs);
  void relink_group_inputs(const NodeTreeRef &group_ref,
                           ArrayRef<DNode *> nodes_by_id,
                           DNode &group_node);
  void relink_group_outputs(const NodeTreeRef &group_ref,
                            ArrayRef<DNode *> nodes_by_id,
                            DNode &group_node);
  void remove_expanded_group_interfaces(Vector<DNode *> &all_nodes);
  void remove_unused_group_inputs(Vector<DGroupInput *> &all_group_inputs);
  void store_in_this_and_init_ids(Vector<DNode *> &&all_nodes,
                                  Vector<DGroupInput *> &&all_group_inputs,
                                  Vector<DParentNode *> &&all_parent_nodes);
};

/* --------------------------------------------------------------------
 * DSocket inline methods.
 */

inline const DNode &DSocket::node() const
{
  return *m_node;
}

inline uint DSocket::id() const
{
  return m_id;
}

inline uint DSocket::index() const
{
  return m_socket_ref->index();
}

inline bool DSocket::is_input() const
{
  return m_socket_ref->is_input();
}

inline bool DSocket::is_output() const
{
  return m_socket_ref->is_output();
}

inline const DSocket &DSocket::as_base() const
{
  return *this;
}

inline const DInputSocket &DSocket::as_input() const
{
  return *(DInputSocket *)this;
}

inline const DOutputSocket &DSocket::as_output() const
{
  return *(DOutputSocket *)this;
}

inline PointerRNA *DSocket::rna() const
{
  return m_socket_ref->rna();
}

inline StringRefNull DSocket::idname() const
{
  return m_socket_ref->idname();
}

inline StringRefNull DSocket::name() const
{
  return m_socket_ref->name();
}

/* --------------------------------------------------------------------
 * DInputSocket inline methods.
 */

inline const InputSocketRef &DInputSocket::socket_ref() const
{
  return m_socket_ref->as_input();
}

inline ArrayRef<const DOutputSocket *> DInputSocket::linked_sockets() const
{
  return m_linked_sockets.as_ref();
}

inline ArrayRef<const DGroupInput *> DInputSocket::linked_group_inputs() const
{
  return m_linked_group_inputs.as_ref();
}

inline bool DInputSocket::is_linked() const
{
  return m_linked_sockets.size() > 0 || m_linked_group_inputs.size() > 0;
}

/* --------------------------------------------------------------------
 * DOutputSocket inline methods.
 */

inline const OutputSocketRef &DOutputSocket::socket_ref() const
{
  return m_socket_ref->as_output();
}

inline ArrayRef<const DInputSocket *> DOutputSocket::linked_sockets() const
{
  return m_linked_sockets.as_ref();
}

/* --------------------------------------------------------------------
 * DGroupInput inline methods.
 */

inline const InputSocketRef &DGroupInput::socket_ref() const
{
  return *m_socket_ref;
}

inline const DParentNode *DGroupInput::parent() const
{
  return m_parent;
}

inline ArrayRef<const DInputSocket *> DGroupInput::linked_sockets() const
{
  return m_linked_sockets.as_ref();
}

inline uint DGroupInput::id() const
{
  return m_id;
}

inline StringRefNull DGroupInput::name() const
{
  return m_socket_ref->name();
}

/* --------------------------------------------------------------------
 * DNode inline methods.
 */

inline const NodeRef &DNode::node_ref() const
{
  return *m_node_ref;
}

inline const DParentNode *DNode::parent() const
{
  return m_parent;
}

inline ArrayRef<const DInputSocket *> DNode::inputs() const
{
  return m_inputs.as_ref();
}

inline ArrayRef<const DOutputSocket *> DNode::outputs() const
{
  return m_outputs.as_ref();
}

inline const DInputSocket &DNode::input(uint index) const
{
  return *m_inputs[index];
}

inline const DOutputSocket &DNode::output(uint index) const
{
  return *m_outputs[index];
}

inline uint DNode::id() const
{
  return m_id;
}

inline PointerRNA *DNode::rna() const
{
  return m_node_ref->rna();
}

inline StringRefNull DNode::idname() const
{
  return m_node_ref->idname();
}

inline StringRefNull DNode::name() const
{
  return m_node_ref->name();
}

/* --------------------------------------------------------------------
 * DParentNode inline methods.
 */

inline const DParentNode *DParentNode::parent() const
{
  return m_parent;
}

inline const NodeRef &DParentNode::node_ref() const
{
  return *m_node_ref;
}

inline uint DParentNode::id() const
{
  return m_id;
}

/* --------------------------------------------------------------------
 * DerivedNodeTree inline methods.
 */

inline ArrayRef<const DNode *> DerivedNodeTree::all_nodes() const
{
  return m_nodes_by_id.as_ref();
}

}  // namespace BKE

#endif /* __BKE_DERIVED_NODE_TREE_HH__ */
