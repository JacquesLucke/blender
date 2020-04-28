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

#ifndef __BKE_NODE_TREE_REF_HH__
#define __BKE_NODE_TREE_REF_HH__

#include "BLI_linear_allocator.hh"
#include "BLI_string_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "RNA_access.h"

namespace BKE {

using BLI::ArrayRef;
using BLI::LinearAllocator;
using BLI::StringMap;
using BLI::StringRef;
using BLI::StringRefNull;
using BLI::Vector;

class SocketRef;
class InputSocketRef;
class OutputSocketRef;
class NodeRef;
class NodeTreeRef;

class SocketRef : BLI::NonCopyable, BLI::NonMovable {
 protected:
  NodeRef *m_node;
  bNodeSocket *m_bsocket;
  bool m_is_input;
  uint m_id;
  uint m_index;
  PointerRNA m_rna;
  Vector<SocketRef *> m_linked_sockets;
  Vector<SocketRef *> m_directly_linked_sockets;

  friend NodeTreeRef;

 public:
  ArrayRef<const SocketRef *> linked_sockets() const;
  ArrayRef<const SocketRef *> directly_linked_sockets() const;
  bool is_linked() const;

  const NodeRef &node() const;
  const NodeTreeRef &tree() const;

  uint id() const;
  uint index() const;

  bool is_input() const;
  bool is_output() const;

  const SocketRef &as_base() const;
  const InputSocketRef &as_input() const;
  const OutputSocketRef &as_output() const;

  PointerRNA *rna() const;

  StringRefNull idname() const;
  StringRefNull name() const;

  bNodeSocket *bsocket() const;
  bNode *bnode() const;
  bNodeTree *ntree() const;
};

class InputSocketRef final : public SocketRef {
 public:
  ArrayRef<const OutputSocketRef *> linked_sockets() const;
  ArrayRef<const OutputSocketRef *> directly_linked_sockets() const;
};

class OutputSocketRef final : public SocketRef {
 public:
  ArrayRef<const InputSocketRef *> linked_sockets() const;
  ArrayRef<const InputSocketRef *> directly_linked_sockets() const;
};

class NodeRef : BLI::NonCopyable, BLI::NonMovable {
 private:
  NodeTreeRef *m_tree;
  bNode *m_bnode;
  PointerRNA m_rna;
  uint m_id;
  Vector<InputSocketRef *> m_inputs;
  Vector<OutputSocketRef *> m_outputs;

  friend NodeTreeRef;

 public:
  const NodeTreeRef &tree() const;

  ArrayRef<const InputSocketRef *> inputs() const;
  ArrayRef<const OutputSocketRef *> outputs() const;

  const InputSocketRef &input(uint index) const;
  const OutputSocketRef &output(uint index) const;

  bNode *bnode() const;
  bNodeTree *btree() const;

  PointerRNA *rna() const;
  StringRefNull idname() const;
  StringRefNull name() const;

  uint id() const;
};

class NodeTreeRef : BLI::NonCopyable, BLI::NonMovable {
 private:
  LinearAllocator<> m_allocator;
  bNodeTree *m_btree;
  Vector<NodeRef *> m_nodes_by_id;
  Vector<SocketRef *> m_sockets_by_id;
  Vector<InputSocketRef *> m_input_sockets;
  Vector<OutputSocketRef *> m_output_sockets;
  StringMap<NodeRef *> m_nodes_by_idname;

 public:
  NodeTreeRef(bNodeTree *btree);
  ~NodeTreeRef();

  ArrayRef<const NodeRef *> nodes() const;
  ArrayRef<const NodeRef *> nodes_with_idname(StringRef idname) const;

  ArrayRef<const SocketRef *> sockets() const;
  ArrayRef<const InputSocketRef *> input_sockets() const;
  ArrayRef<const OutputSocketRef *> output_sockets() const;

  bNodeTree *btree() const;
};

}  // namespace BKE

#endif /* __BKE_NODE_TREE_REF_HH__ */
