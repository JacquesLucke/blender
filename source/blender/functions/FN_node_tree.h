#ifndef __BKE_INLINED_NODE_TREE_H__
#define __BKE_INLINED_NODE_TREE_H__

#include "BKE_virtual_node_tree.h"

#include "BLI_map.h"
#include "BLI_multi_map.h"

namespace FN {

using BKE::VInputSocket;
using BKE::VirtualNodeTree;
using BKE::VNode;
using BKE::VOutputSocket;
using BKE::VSocket;
using BLI::ArrayRef;
using BLI::Map;
using BLI::MultiMap;
using BLI::MutableArrayRef;
using BLI::StringMap;
using BLI::StringMultiMap;
using BLI::StringRef;
using BLI::StringRefNull;
using BLI::Vector;

class FNode;
class FParentNode;
class FSocket;
class FInputSocket;
class FOutputSocket;
class FGroupInput;
class FunctionTree;

class FSocket : BLI::NonCopyable, BLI::NonMovable {
 protected:
  FNode *m_node;
  const VSocket *m_vsocket;
  bool m_is_input;

  /* Input and output sockets share the same id-space. */
  uint m_id;

  friend FunctionTree;

 public:
  const FNode &node() const;
  uint id() const;

  bool is_input() const;
  bool is_output() const;
  const FSocket &as_base() const;
  const FInputSocket &as_input() const;
  const FOutputSocket &as_output() const;

  PointerRNA *rna() const;
  StringRefNull idname() const;
  StringRefNull name() const;

  uint index() const;
};

class FInputSocket : public FSocket {
 private:
  Vector<FOutputSocket *> m_linked_sockets;
  Vector<FGroupInput *> m_linked_group_inputs;

  friend FunctionTree;

 public:
  const VInputSocket &vsocket() const;
  ArrayRef<const FOutputSocket *> linked_sockets() const;
  ArrayRef<const FGroupInput *> linked_group_inputs() const;

  bool is_linked() const;
};

class FOutputSocket : public FSocket {
 private:
  Vector<FInputSocket *> m_linked_sockets;

  friend FunctionTree;

 public:
  const VOutputSocket &vsocket() const;
  ArrayRef<const FInputSocket *> linked_sockets() const;
};

class FGroupInput : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VInputSocket *m_vsocket;
  FParentNode *m_parent;
  Vector<FInputSocket *> m_linked_sockets;
  uint m_id;

  friend FunctionTree;

 public:
  const VInputSocket &vsocket() const;
  const FParentNode *parent() const;
  ArrayRef<const FInputSocket *> linked_sockets() const;
  uint id() const;
};

class FNode : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VNode *m_vnode;
  FParentNode *m_parent;

  Vector<FInputSocket *> m_inputs;
  Vector<FOutputSocket *> m_outputs;

  /* Uniquely identifies this node in the inlined node tree. */
  uint m_id;

  friend FunctionTree;

  void destruct_with_sockets();

 public:
  const VNode &vnode() const;
  const FParentNode *parent() const;

  ArrayRef<const FInputSocket *> inputs() const;
  ArrayRef<const FOutputSocket *> outputs() const;

  const FInputSocket &input(uint index) const;
  const FOutputSocket &output(uint index) const;
  const FInputSocket &input(uint index, StringRef expected_name) const;
  const FOutputSocket &output(uint index, StringRef expected_name) const;

  uint id() const;

  PointerRNA *rna() const;
  StringRefNull idname() const;
  StringRefNull name() const;

  const FInputSocket *input_with_name_prefix(StringRef name_prefix) const;
};

class FParentNode : BLI::NonCopyable, BLI::NonMovable {
 private:
  const VNode *m_vnode;
  FParentNode *m_parent;
  uint m_id;

  friend FunctionTree;

 public:
  const FParentNode *parent() const;
  const VNode &vnode() const;
  uint id() const;
};

using BTreeVTreeMap = Map<bNodeTree *, std::unique_ptr<const VirtualNodeTree>>;

class FunctionTree : BLI::NonCopyable, BLI::NonMovable {
 private:
  BLI::MonotonicAllocator<> m_allocator;
  bNodeTree *m_btree;
  Vector<FNode *> m_node_by_id;
  Vector<FGroupInput *> m_group_inputs;
  Vector<FParentNode *> m_parent_nodes;

  Vector<FSocket *> m_sockets_by_id;
  Vector<FInputSocket *> m_input_sockets;
  Vector<FOutputSocket *> m_output_sockets;

  StringMultiMap<FNode *> m_nodes_by_idname;

 public:
  FunctionTree(bNodeTree *btree, BTreeVTreeMap &vtrees);
  ~FunctionTree();

  std::string to_dot() const;
  void to_dot__clipboard() const;

  const FSocket &socket_by_id(uint id) const;
  uint socket_count() const;
  uint node_count() const;

  ArrayRef<const FSocket *> all_sockets() const;
  ArrayRef<const FNode *> all_nodes() const;
  ArrayRef<const FInputSocket *> all_input_sockets() const;
  ArrayRef<const FOutputSocket *> all_output_sockets() const;
  ArrayRef<const FGroupInput *> all_group_inputs() const;
  ArrayRef<const FNode *> nodes_with_idname(StringRef idname) const;

 private:
  void expand_groups(Vector<FNode *> &all_nodes,
                     Vector<FGroupInput *> &all_group_inputs,
                     Vector<FParentNode *> &all_parent_nodes,
                     BTreeVTreeMap &vtrees);
  void expand_group_node(FNode &group_node,
                         Vector<FNode *> &all_nodes,
                         Vector<FGroupInput *> &all_group_inputs,
                         Vector<FParentNode *> &all_parent_nodes,
                         BTreeVTreeMap &vtrees);
  void expand_group__group_inputs_for_unlinked_inputs(FNode &group_node,
                                                      Vector<FGroupInput *> &all_group_inputs);
  void expand_group__relink_inputs(const VirtualNodeTree &vtree,
                                   ArrayRef<FNode *> new_fnodes_by_id,
                                   FNode &group_node);
  void expand_group__relink_outputs(const VirtualNodeTree &vtree,
                                    ArrayRef<FNode *> new_fnodes_by_id,
                                    FNode &group_node);
  void insert_linked_nodes_for_vtree_in_id_order(const VirtualNodeTree &vtree,
                                                 Vector<FNode *> &all_nodes,
                                                 FParentNode *parent);
  FNode &create_node(const VNode &vnode,
                     FParentNode *parent,
                     MutableArrayRef<FSocket *> sockets_map);
  void remove_expanded_groups_and_interfaces(Vector<FNode *> &all_nodes);
  void store_tree_in_this_and_init_ids(Vector<FNode *> &&all_nodes,
                                       Vector<FGroupInput *> &&all_group_inputs,
                                       Vector<FParentNode *> &&all_parent_nodes);
};

/* Inline functions
 ********************************************/

inline const VNode &FNode::vnode() const
{
  return *m_vnode;
}

inline const FParentNode *FNode::parent() const
{
  return m_parent;
}

inline ArrayRef<const FInputSocket *> FNode::inputs() const
{
  return m_inputs.as_ref();
}

inline ArrayRef<const FOutputSocket *> FNode::outputs() const
{
  return m_outputs.as_ref();
}

inline const FInputSocket &FNode::input(uint index) const
{
  return *m_inputs[index];
}

inline const FOutputSocket &FNode::output(uint index) const
{
  return *m_outputs[index];
}

inline const FInputSocket &FNode::input(uint index, StringRef expected_name) const
{
  BLI_assert(m_inputs[index]->name() == expected_name);
  UNUSED_VARS_NDEBUG(expected_name);
  return *m_inputs[index];
}

inline const FOutputSocket &FNode::output(uint index, StringRef expected_name) const
{
  BLI_assert(m_outputs[index]->name() == expected_name);
  UNUSED_VARS_NDEBUG(expected_name);
  return *m_outputs[index];
}

inline uint FNode::id() const
{
  return m_id;
}

inline PointerRNA *FNode::rna() const
{
  return m_vnode->rna();
}

inline StringRefNull FNode::idname() const
{
  return m_vnode->idname();
}

inline StringRefNull FNode::name() const
{
  return m_vnode->name();
}

inline const FParentNode *FParentNode::parent() const
{
  return m_parent;
}

inline const VNode &FParentNode::vnode() const
{
  return *m_vnode;
}

inline uint FParentNode::id() const
{
  return m_id;
}

inline const FNode &FSocket::node() const
{
  return *m_node;
}

inline uint FSocket::id() const
{
  return m_id;
}

inline bool FSocket::is_input() const
{
  return m_is_input;
}

inline bool FSocket::is_output() const
{
  return !m_is_input;
}

inline const FSocket &FSocket::as_base() const
{
  return *this;
}

inline const FInputSocket &FSocket::as_input() const
{
  BLI_assert(this->is_input());
  return *(const FInputSocket *)this;
}

inline const FOutputSocket &FSocket::as_output() const
{
  BLI_assert(this->is_output());
  return *(const FOutputSocket *)this;
}

inline PointerRNA *FSocket::rna() const
{
  return m_vsocket->rna();
}

inline StringRefNull FSocket::idname() const
{
  return m_vsocket->idname();
}

inline StringRefNull FSocket::name() const
{
  return m_vsocket->name();
}

inline uint FSocket::index() const
{
  return m_vsocket->index();
}

inline const VInputSocket &FInputSocket::vsocket() const
{
  return m_vsocket->as_input();
}

inline ArrayRef<const FOutputSocket *> FInputSocket::linked_sockets() const
{
  return m_linked_sockets.as_ref();
}

inline ArrayRef<const FGroupInput *> FInputSocket::linked_group_inputs() const
{
  return m_linked_group_inputs.as_ref();
}

inline bool FInputSocket::is_linked() const
{
  return m_linked_sockets.size() > 0 || m_linked_group_inputs.size() > 0;
}

inline const VOutputSocket &FOutputSocket::vsocket() const
{
  return m_vsocket->as_output();
}

inline ArrayRef<const FInputSocket *> FOutputSocket::linked_sockets() const
{
  return m_linked_sockets.as_ref();
}

inline const VInputSocket &FGroupInput::vsocket() const
{
  return *m_vsocket;
}

inline const FParentNode *FGroupInput::parent() const
{
  return m_parent;
}

inline ArrayRef<const FInputSocket *> FGroupInput::linked_sockets() const
{
  return m_linked_sockets.as_ref();
}

inline uint FGroupInput::id() const
{
  return m_id;
}

inline const FSocket &FunctionTree::socket_by_id(uint id) const
{
  return *m_sockets_by_id[id];
}

inline uint FunctionTree::socket_count() const
{
  return m_sockets_by_id.size();
}

inline uint FunctionTree::node_count() const
{
  return m_node_by_id.size();
}

inline ArrayRef<const FSocket *> FunctionTree::all_sockets() const
{
  return m_sockets_by_id.as_ref();
}

inline ArrayRef<const FNode *> FunctionTree::all_nodes() const
{
  return m_node_by_id.as_ref();
}

inline ArrayRef<const FInputSocket *> FunctionTree::all_input_sockets() const
{
  return m_input_sockets.as_ref();
}

inline ArrayRef<const FOutputSocket *> FunctionTree::all_output_sockets() const
{
  return m_output_sockets.as_ref();
}

inline ArrayRef<const FGroupInput *> FunctionTree::all_group_inputs() const
{
  return m_group_inputs.as_ref();
}

inline ArrayRef<const FNode *> FunctionTree::nodes_with_idname(StringRef idname) const
{
  return m_nodes_by_idname.lookup_default(idname);
}

}  // namespace FN

#endif /* __BKE_INLINED_NODE_TREE_H__ */
