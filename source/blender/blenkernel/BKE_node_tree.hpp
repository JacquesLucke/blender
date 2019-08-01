#pragma once

#include "DNA_node_types.h"

#include "BLI_string_ref.hpp"
#include "BLI_array_ref.hpp"
#include "BLI_map.hpp"
#include "BLI_vector.hpp"
#include "BLI_listbase_wrapper.hpp"
#include "BLI_multimap.hpp"
#include "BLI_monotonic_allocator.hpp"

#include "RNA_access.h"

namespace BKE {

using BLI::ArrayRef;
using BLI::ListBaseWrapper;
using BLI::Map;
using BLI::MonotonicAllocator;
using BLI::MultiMap;
using BLI::StringRef;
using BLI::StringRefNull;
using BLI::Vector;

using bNodeList = ListBaseWrapper<struct bNode *, true>;
using bLinkList = ListBaseWrapper<struct bNodeLink *, true>;
using bSocketList = ListBaseWrapper<struct bNodeSocket *, true>;

class VirtualNode;
class VirtualSocket;
class VirtualLink;

class VirtualNodeTree {
 private:
  bool m_frozen = false;
  Vector<VirtualNode *> m_nodes;
  Vector<VirtualLink *> m_links;
  Vector<VirtualSocket *> m_inputs_with_links;
  MultiMap<std::string, VirtualNode *> m_nodes_by_idname;
  MonotonicAllocator<> m_allocator;
  uint m_socket_counter = 0;

 public:
  void add_all_of_tree(bNodeTree *btree);
  VirtualNode *add_bnode(bNodeTree *btree, bNode *bnode);
  void add_link(VirtualSocket *a, VirtualSocket *b);

  void freeze_and_index();

  ArrayRef<VirtualNode *> nodes()
  {
    return m_nodes;
  }

  ArrayRef<VirtualLink *> links()
  {
    return m_links;
  }

  ArrayRef<VirtualSocket *> inputs_with_links()
  {
    BLI_assert(m_frozen);
    return m_inputs_with_links;
  }

  ArrayRef<VirtualNode *> nodes_with_idname(StringRef idname)
  {
    BLI_assert(m_frozen);
    return m_nodes_by_idname.lookup_default(idname.to_std_string());
  }

  bool is_frozen()
  {
    return m_frozen;
  }

  uint socket_count()
  {
    return m_socket_counter;
  }

 private:
  void initialize_direct_links();
  void initialize_links();
  void initialize_nodes_by_idname();
};

class VirtualNode {
 private:
  friend VirtualNodeTree;
  friend VirtualSocket;

  VirtualNodeTree *m_backlink;
  bNodeTree *m_btree;
  bNode *m_bnode;
  ArrayRef<VirtualSocket *> m_inputs;
  ArrayRef<VirtualSocket *> m_outputs;

 public:
  ArrayRef<VirtualSocket *> inputs()
  {
    return m_inputs;
  }

  ArrayRef<VirtualSocket *> outputs()
  {
    return m_outputs;
  }

  VirtualSocket *input(uint index)
  {
    return m_inputs[index];
  }

  VirtualSocket *output(uint index)
  {
    return m_outputs[index];
  }

  bNode *bnode()
  {
    return m_bnode;
  }

  bNodeTree *btree()
  {
    return m_btree;
  }

  ID *btree_id()
  {
    return &m_btree->id;
  }

  PointerRNA rna()
  {
    PointerRNA rna;
    RNA_pointer_create(&m_btree->id, &RNA_Node, m_bnode, &rna);
    return rna;
  }

  const char *name()
  {
    return m_bnode->name;
  }

  const char *idname()
  {
    return m_bnode->idname;
  }
};

class VirtualSocket {
 private:
  friend VirtualNodeTree;

  VirtualNode *m_vnode;
  bNodeTree *m_btree;
  bNodeSocket *m_bsocket;
  uint m_id;

  ArrayRef<VirtualSocket *> m_direct_links;
  ArrayRef<VirtualSocket *> m_links;

 public:
  bool is_input() const
  {
    return this->m_bsocket->in_out == SOCK_IN;
  }

  bool is_output() const
  {
    return this->m_bsocket->in_out == SOCK_OUT;
  }

  bNodeSocket *bsocket()
  {
    return m_bsocket;
  }

  bNodeTree *btree()
  {
    return m_btree;
  }

  uint id()
  {
    return m_id;
  }

  ID *btree_id()
  {
    return &m_btree->id;
  }

  VirtualNode *vnode()
  {
    return m_vnode;
  }

  ArrayRef<VirtualSocket *> direct_links()
  {
    BLI_assert(m_vnode->m_backlink->is_frozen());
    return m_direct_links;
  }

  ArrayRef<VirtualSocket *> links()
  {
    BLI_assert(m_vnode->m_backlink->is_frozen());
    return m_links;
  }

  PointerRNA rna()
  {
    PointerRNA rna;
    RNA_pointer_create(&m_btree->id, &RNA_NodeSocket, m_bsocket, &rna);
    return rna;
  }

  const char *name()
  {
    return m_bsocket->name;
  }

  const char *idname()
  {
    return m_bsocket->idname;
  }

  const char *identifier()
  {
    return m_bsocket->identifier;
  }
};

class VirtualLink {
 private:
  friend VirtualNodeTree;

  VirtualSocket *m_from;
  VirtualSocket *m_to;
};

}  // namespace BKE
