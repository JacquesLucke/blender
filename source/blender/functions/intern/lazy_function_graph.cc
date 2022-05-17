/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_lazy_function_graph.hh"

namespace blender::fn {

LazyFunctionGraph::~LazyFunctionGraph()
{
  for (LFNode *node : nodes_) {
    std::destroy_at(node);
  }
}

LFNode &LazyFunctionGraph::add_node(const LazyFunction &fn)
{
  const Span<LazyFunctionInput> inputs = fn.inputs();
  const Span<LazyFunctionOutput> outputs = fn.outputs();

  LFNode &node = *allocator_.construct<LFNode>().release();
  node.fn_ = &fn;
  node.inputs_ = allocator_.construct_elements_and_pointer_array<LFInputSocket>(inputs.size());
  node.outputs_ = allocator_.construct_elements_and_pointer_array<LFOutputSocket>(outputs.size());

  for (const int i : inputs.index_range()) {
    LFInputSocket &socket = *node.inputs_[i];
    socket.index_in_node_ = i;
    socket.is_input_ = true;
    socket.node_ = &node;
  }
  for (const int i : outputs.index_range()) {
    LFOutputSocket &socket = *node.outputs_[i];
    socket.index_in_node_ = i;
    socket.is_input_ = false;
    socket.node_ = &node;
  }

  nodes_.append(&node);
  return node;
}

void LazyFunctionGraph::add_link(LFOutputSocket &from, LFInputSocket &to)
{
  BLI_assert(to.origin_ == nullptr);
  to.origin_ = &from;
  from.targets_.append(&to);
}

}  // namespace blender::fn
