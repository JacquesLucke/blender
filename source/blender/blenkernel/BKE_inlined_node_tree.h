#ifndef __BKE_INLINED_NODE_TREE_H__
#define __BKE_INLINED_NODE_TREE_H__

#include "BKE_virtual_node_tree.h"

#include "BLI_map.h"
#include "BLI_multi_map.h"

namespace BKE {

using BLI::Map;
using BLI::MultiMap;
using BLI::MutableArrayRef;

class XNode;
class XParentNode;
class XSocket;
class XInputSocket;
class XOutputSocket;
class XGroupInput;
class InlinedNodeTree;

class XSocket : BLI::NonCopyable, BLI::NonMovable {
 protected:
  XNode *m_node;
  bool m_is_input;

  /* Input and output sockets share the same id-space. */
  uint m_id;

  friend InlinedNodeTree;

 public:
  const XNode &node() const;
  uint id() const;

  bool is_input() const;
  bool is_output() const;
  const XInputSocket &as_input() const;
  const XOutputSocket &as_output() const;
};

class XInputSocket : public XSocket {
 private:
  const VInputSocket *m_vsocket;
  Vector<XOutputSocket *> m_linked_sockets;
  Vector<XGroupInput *> m_linked_group_inputs;

  friend InlinedNodeTree;

 public:
  const VInputSocket &vsocket() const;
  ArrayRef<const XOutputSocket *> linked_sockets() const;
  ArrayRef<const XGroupInput *> linked_group_inputs() const;
};

class XOutputSocket : public XSocket {
 private:
  const VOutputSocket *m_vsocket;
  Vector<XInputSocket *> m_linked_sockets;

  friend InlinedNodeTree;

 public:
  const VOutputSocket &vsocket() const;
  ArrayRef<const XInputSocket *> linked_sockets() const;
};

class XGroupInput : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VInputSocket *m_vsocket;
  XParentNode *m_parent;
  Vector<XInputSocket *> m_linked_sockets;
  uint m_id;

  friend InlinedNodeTree;

 public:
  const VInputSocket &vsocket() const;
  const XParentNode *parent() const;
  ArrayRef<const XInputSocket *> linked_sockets() const;
  uint id() const;
};

class XNode : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VNode *m_vnode;
  XParentNode *m_parent;

  Vector<XInputSocket *> m_inputs;
  Vector<XOutputSocket *> m_outputs;

  /* Uniquely identifies this node in the inlined node tree. */
  uint m_id;

  friend InlinedNodeTree;

  void destruct_with_sockets();

 public:
  const VNode &vnode() const;
  const XParentNode *parent() const;

  ArrayRef<const XInputSocket *> inputs() const;
  ArrayRef<const XOutputSocket *> outputs() const;

  const XInputSocket &input(uint index) const;
  const XOutputSocket &output(uint index) const;

  uint id() const;
};

class XParentNode : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VNode *m_vnode;
  XParentNode *m_parent;
  uint m_id;

  friend InlinedNodeTree;

 public:
  const XParentNode *parent() const;
  const VNode &vnode() const;
  uint id() const;
};

using BTreeVTreeMap = Map<bNodeTree *, std::unique_ptr<const VirtualNodeTree>>;

class InlinedNodeTree : BLI::NonCopyable, BLI::NonMovable {
 private:
  BLI::MonotonicAllocator<> m_allocator;
  bNodeTree *m_btree;
  Vector<XNode *> m_node_by_id;
  Vector<XGroupInput *> m_group_inputs;
  Vector<XParentNode *> m_parent_nodes;

  Vector<XSocket *> m_sockets_by_id;
  Vector<XInputSocket *> m_input_sockets;
  Vector<XOutputSocket *> m_output_sockets;

 public:
  InlinedNodeTree(bNodeTree *btree, BTreeVTreeMap &vtrees);
  ~InlinedNodeTree();

  std::string to_dot() const;
  void to_dot__clipboard() const;

 private:
  void expand_group_node(XNode &group_node,
                         Vector<XNode *> &all_nodes,
                         Vector<XGroupInput *> &all_group_inputs,
                         Vector<XParentNode *> &all_parent_nodes,
                         BTreeVTreeMap &vtrees);
  void insert_linked_nodes_for_vtree_in_id_order(const VirtualNodeTree &vtree,
                                                 Vector<XNode *> &all_nodes,
                                                 XParentNode *parent);
  XNode &create_node(const VNode &vnode,
                     XParentNode *parent,
                     MutableArrayRef<XSocket *> sockets_map);
};

/* Inline functions
 ********************************************/

inline const VNode &XNode::vnode() const
{
  return *m_vnode;
}

inline const XParentNode *XNode::parent() const
{
  return m_parent;
}

inline ArrayRef<const XInputSocket *> XNode::inputs() const
{
  return m_inputs.as_ref();
}

inline ArrayRef<const XOutputSocket *> XNode::outputs() const
{
  return m_outputs.as_ref();
}

inline const XInputSocket &XNode::input(uint index) const
{
  return *m_inputs[index];
}

inline const XOutputSocket &XNode::output(uint index) const
{
  return *m_outputs[index];
}

inline uint XNode::id() const
{
  return m_id;
}

inline const XParentNode *XParentNode::parent() const
{
  return m_parent;
}

inline const VNode &XParentNode::vnode() const
{
  return *m_vnode;
}

inline uint XParentNode::id() const
{
  return m_id;
}

inline const XNode &XSocket::node() const
{
  return *m_node;
}

inline uint XSocket::id() const
{
  return m_id;
}

inline bool XSocket::is_input() const
{
  return m_is_input;
}

inline bool XSocket::is_output() const
{
  return !m_is_input;
}

inline const XInputSocket &XSocket::as_input() const
{
  BLI_assert(this->is_input());
  return *(const XInputSocket *)this;
}

inline const XOutputSocket &XSocket::as_output() const
{
  BLI_assert(this->is_output());
  return *(const XOutputSocket *)this;
}

inline const VInputSocket &XInputSocket::vsocket() const
{
  return *m_vsocket;
}

inline ArrayRef<const XOutputSocket *> XInputSocket::linked_sockets() const
{
  return m_linked_sockets.as_ref();
}

inline ArrayRef<const XGroupInput *> XInputSocket::linked_group_inputs() const
{
  return m_linked_group_inputs.as_ref();
}

inline const VOutputSocket &XOutputSocket::vsocket() const
{
  return *m_vsocket;
}

inline ArrayRef<const XInputSocket *> XOutputSocket::linked_sockets() const
{
  return m_linked_sockets.as_ref();
}

inline const VInputSocket &XGroupInput::vsocket() const
{
  return *m_vsocket;
}

inline const XParentNode *XGroupInput::parent() const
{
  return m_parent;
}

inline ArrayRef<const XInputSocket *> XGroupInput::linked_sockets() const
{
  return m_linked_sockets.as_ref();
}

inline uint XGroupInput::id() const
{
  return m_id;
}

}  // namespace BKE

#endif /* __BKE_INLINED_NODE_TREE_H__ */
