#ifndef __BKE_INLINED_NODE_TREE_H__
#define __BKE_INLINED_NODE_TREE_H__

#include "BKE_virtual_node_tree.h"

#include "BLI_map.h"
#include "BLI_multi_map.h"

namespace BKE {

using BLI::Map;
using BLI::MultiMap;

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
  uint m_id;

  friend InlinedNodeTree;

 public:
  const XNode &node() const;
};

class XInputSocket : public XSocket {
 private:
  const VInputSocket *m_vsocket;
  Vector<XOutputSocket *> m_linked_sockets;
  Vector<XGroupInput *> m_linked_group_inputs;

  friend InlinedNodeTree;
};

class XOutputSocket : public XSocket {
 private:
  const VOutputSocket *m_vsocket;
  Vector<XInputSocket *> m_linked_sockets;

  friend InlinedNodeTree;
};

class XGroupInput : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VInputSocket *m_vsocket;
  XParentNode *m_parent;
  Vector<XInputSocket *> m_linked_sockets;

  friend InlinedNodeTree;
};

class XNode : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VNode *m_vnode;
  XParentNode *m_parent;
  uint m_id;
  Vector<XInputSocket *> m_inputs;
  Vector<XOutputSocket *> m_outputs;

  friend InlinedNodeTree;
};

class XParentNode : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VNode *m_vnode;
  XParentNode *m_parent;

  friend InlinedNodeTree;
};

using BTreeVTreeMap = Map<bNodeTree *, std::unique_ptr<const VirtualNodeTree>>;

class InlinedNodeTree : BLI::NonCopyable, BLI::NonMovable {
 private:
  BLI::MonotonicAllocator<> m_allocator;
  bNodeTree *m_btree;
  Vector<XNode *> m_node_by_id;
  Vector<XSocket *> m_sockets_by_id;
  Vector<XInputSocket *> m_input_sockets;
  Vector<XOutputSocket *> m_output_sockets;
  Vector<XParentNode *> m_parent_nodes;

 public:
  InlinedNodeTree(bNodeTree *btree, BTreeVTreeMap &vtrees);

  std::string to_dot() const;
  void to_dot__clipboard() const;

 private:
  void expand_group_node(XNode &group_node, Vector<XNode *> &nodes, BTreeVTreeMap &vtrees);
  XNode &create_node(const VNode &vnode,
                     XParentNode *parent,
                     Map<const VInputSocket *, XInputSocket *> &inputs_map,
                     Map<const VOutputSocket *, XOutputSocket *> &outputs_map);
};

/* Inline functions
 ********************************************/

inline const XNode &XSocket::node() const
{
  return *m_node;
}

}  // namespace BKE

#endif /* __BKE_INLINED_NODE_TREE_H__ */
