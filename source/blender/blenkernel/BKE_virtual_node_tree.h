#ifndef __BKE_VIRTUAL_NODE_TREE_H__
#define __BKE_VIRTUAL_NODE_TREE_H__

#include "BLI_vector.h"
#include "BLI_utility_mixins.h"
#include "BLI_array_cxx.h"
#include "BLI_string_ref.h"
#include "BLI_string_map.h"
#include "BLI_resource_collector.h"

#include "DNA_node_types.h"

#include "RNA_access.h"

namespace BKE {

using BLI::Array;
using BLI::ArrayRef;
using BLI::ResourceCollector;
using BLI::StringMap;
using BLI::StringRef;
using BLI::StringRefNull;
using BLI::Vector;

class VSocket;
class VInputSocket;
class VOutputSocket;
class VNode;
class VirtualNodeTree;

/* Virtual Node Tree declarations
 ******************************************/

class VSocket : BLI::NonCopyable, BLI::NonMovable {
 protected:
  Vector<VSocket *> m_linked_sockets;
  Vector<VSocket *> m_directly_linked_sockets;
  VNode *m_node;
  bool m_is_input;
  bNodeSocket *m_bsocket;
  uint m_id;
  PointerRNA m_rna;
  uint m_index;

  friend VirtualNodeTree;

 public:
  ArrayRef<const VSocket *> linked_sockets() const;
  ArrayRef<const VSocket *> directly_linked_sockets() const;

  const VNode &node() const;
  const VirtualNodeTree &tree() const;
  uint id() const;

  uint index() const;

  bool is_input() const;
  bool is_output() const;

  const VSocket &as_base() const;
  const VInputSocket &as_input() const;
  const VOutputSocket &as_output() const;

  PointerRNA *rna() const;

  StringRefNull idname() const;
  StringRefNull name() const;

  bool is_linked() const;

  bNodeSocket *bsocket() const;
  bNodeTree *btree() const;
};

class VInputSocket final : public VSocket {
 public:
  ArrayRef<const VOutputSocket *> linked_sockets() const;
  ArrayRef<const VOutputSocket *> directly_linked_sockets() const;
};

class VOutputSocket final : public VSocket {
 public:
  ArrayRef<const VInputSocket *> linked_sockets() const;
  ArrayRef<const VInputSocket *> directly_linked_sockets() const;
};

class VNode : BLI::NonCopyable, BLI::NonMovable {
 private:
  VirtualNodeTree *m_vtree;
  Vector<VInputSocket *> m_inputs;
  Vector<VOutputSocket *> m_outputs;
  bNode *m_bnode;
  uint m_id;
  PointerRNA m_rna;

  friend VirtualNodeTree;

 public:
  const VirtualNodeTree &tree() const;

  ArrayRef<const VInputSocket *> inputs() const;
  ArrayRef<const VOutputSocket *> outputs() const;

  PointerRNA *rna() const;
  StringRefNull idname() const;
  StringRefNull name() const;

  const VInputSocket &input(uint index) const;
  const VOutputSocket &output(uint index) const;

  const VInputSocket &input(uint index, StringRef expected_name) const;
  const VOutputSocket &output(uint index, StringRef expected_name) const;

  bNode *bnode() const;
  bNodeTree *btree() const;
};

class VirtualNodeTree : BLI::NonCopyable, BLI::NonMovable {
 private:
  BLI::MonotonicAllocator<> m_allocator;
  bNodeTree *m_btree;
  Vector<VNode *> m_nodes_by_id;
  Vector<VSocket *> m_sockets_by_id;
  Vector<VInputSocket *> m_input_sockets;
  Vector<VOutputSocket *> m_output_sockets;
  StringMap<Vector<VNode *>> m_nodes_by_idname;

 public:
  VirtualNodeTree(bNodeTree *btree);
  ~VirtualNodeTree();

  ArrayRef<const VNode *> nodes() const;
  ArrayRef<const VNode *> nodes_with_idname(StringRef idname) const;
  uint socket_count() const;

  ArrayRef<const VSocket *> all_sockets() const;
  ArrayRef<const VInputSocket *> all_input_sockets() const;

  const VSocket &socket_by_id(uint id) const;

  bNodeTree *btree() const;

 private:
  void find_targets_skipping_reroutes(VOutputSocket &vsocket, Vector<VSocket *> &r_targets);
};

/* Virtual Node Tree inline functions
 ****************************************************/

inline ArrayRef<const VSocket *> VSocket::linked_sockets() const
{
  return ArrayRef<VSocket *>(m_linked_sockets).cast<const VSocket *>();
}

inline ArrayRef<const VSocket *> VSocket::directly_linked_sockets() const
{
  return ArrayRef<VSocket *>(m_directly_linked_sockets).cast<const VSocket *>();
}

inline const VirtualNodeTree &VSocket::tree() const
{
  return m_node->tree();
}

inline const VNode &VSocket::node() const
{
  return *m_node;
}

inline uint VSocket::id() const
{
  return m_id;
}

inline uint VSocket::index() const
{
  return m_index;
}

inline bool VSocket::is_input() const
{
  return m_is_input;
}

inline bool VSocket::is_output() const
{
  return !m_is_input;
}

inline bool VSocket::is_linked() const
{
  return m_linked_sockets.size() > 0;
}

inline const VSocket &VSocket::as_base() const
{
  return *this;
}

inline const VInputSocket &VSocket::as_input() const
{
  BLI_assert(this->is_input());
  return *(const VInputSocket *)this;
}

inline const VOutputSocket &VSocket::as_output() const
{
  BLI_assert(this->is_output());
  return *(const VOutputSocket *)this;
}

inline PointerRNA *VSocket::rna() const
{
  return const_cast<PointerRNA *>(&m_rna);
}

inline StringRefNull VSocket::idname() const
{
  return m_bsocket->idname;
}

inline StringRefNull VSocket::name() const
{
  return m_bsocket->name;
}

inline bNodeSocket *VSocket::bsocket() const
{
  return m_bsocket;
}

inline ArrayRef<const VOutputSocket *> VInputSocket::linked_sockets() const
{
  return ArrayRef<VSocket *>(m_linked_sockets).cast<const VOutputSocket *>();
}

inline ArrayRef<const VOutputSocket *> VInputSocket::directly_linked_sockets() const
{
  return ArrayRef<VSocket *>(m_directly_linked_sockets).cast<const VOutputSocket *>();
}

inline ArrayRef<const VInputSocket *> VOutputSocket::linked_sockets() const
{
  return ArrayRef<VSocket *>(m_linked_sockets).cast<const VInputSocket *>();
}

inline ArrayRef<const VInputSocket *> VOutputSocket::directly_linked_sockets() const
{
  return ArrayRef<VSocket *>(m_directly_linked_sockets).cast<const VInputSocket *>();
}

inline ArrayRef<const VInputSocket *> VNode::inputs() const
{
  return ArrayRef<VInputSocket *>(m_inputs).cast<const VInputSocket *>();
}

inline ArrayRef<const VOutputSocket *> VNode::outputs() const
{
  return ArrayRef<VOutputSocket *>(m_outputs).cast<const VOutputSocket *>();
}

inline const VirtualNodeTree &VNode::tree() const
{
  return *m_vtree;
}

inline PointerRNA *VNode::rna() const
{
  return const_cast<PointerRNA *>(&m_rna);
}

inline StringRefNull VNode::idname() const
{
  return m_bnode->idname;
}

inline StringRefNull VNode::name() const
{
  return m_bnode->name;
}

inline const VInputSocket &VNode::input(uint index) const
{
  return *m_inputs[index];
}

inline const VOutputSocket &VNode::output(uint index) const
{
  return *m_outputs[index];
}

inline const VInputSocket &VNode::input(uint index, StringRef expected_name) const
{
  BLI_assert(m_inputs[index]->name() == expected_name);
  UNUSED_VARS_NDEBUG(expected_name);
  return *m_inputs[index];
}

inline const VOutputSocket &VNode::output(uint index, StringRef expected_name) const
{
  BLI_assert(m_outputs[index]->name() == expected_name);
  UNUSED_VARS_NDEBUG(expected_name);
  return *m_outputs[index];
}

inline bNode *VNode::bnode() const
{
  return m_bnode;
}

inline bNodeTree *VirtualNodeTree::btree() const
{
  return m_btree;
}

inline ArrayRef<const VNode *> VirtualNodeTree::nodes() const
{
  return ArrayRef<VNode *>(m_nodes_by_id).cast<const VNode *>();
}

inline ArrayRef<const VNode *> VirtualNodeTree::nodes_with_idname(StringRef idname) const
{
  auto *nodes = m_nodes_by_idname.lookup_ptr(idname);
  if (nodes == nullptr) {
    return {};
  }
  else {
    return ArrayRef<VNode *>(*nodes).cast<const VNode *>();
  }
}

inline uint VirtualNodeTree::socket_count() const
{
  return m_sockets_by_id.size();
}

inline ArrayRef<const VSocket *> VirtualNodeTree::all_sockets() const
{
  return ArrayRef<VSocket *>(m_sockets_by_id).cast<const VSocket *>();
}

inline ArrayRef<const VInputSocket *> VirtualNodeTree::all_input_sockets() const
{
  return ArrayRef<VInputSocket *>(m_input_sockets).cast<const VInputSocket *>();
}

inline const VSocket &VirtualNodeTree::socket_by_id(uint id) const
{
  return *m_sockets_by_id[id];
}

}  // namespace BKE

#endif /* __BKE_VIRTUAL_NODE_TREE_H__ */
