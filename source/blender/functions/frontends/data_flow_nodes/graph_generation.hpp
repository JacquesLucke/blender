#pragma once

#include "FN_core.hpp"
#include "BLI_value_or_error.hpp"
#include "BKE_node_tree.hpp"

#include "vtree_data_graph.hpp"
#include "vtree_data_graph_builder.hpp"

namespace FN {
namespace DataFlowNodes {

class UnlinkedInputsHandler {
 public:
  virtual void insert(VTreeDataGraphBuilder &builder,
                      ArrayRef<VirtualSocket *> unlinked_inputs,
                      ArrayRef<DFGB_Socket> r_new_origins) = 0;
};

class VNodePlaceholderBody : public FunctionBody {
 private:
  VirtualNode *m_vnode;

 public:
  static const uint FUNCTION_BODY_ID = 4;

  VNodePlaceholderBody(VirtualNode *vnode) : m_vnode(vnode)
  {
  }

  VirtualNode *vnode()
  {
    return m_vnode;
  }
};

ValueOrError<VTreeDataGraph> generate_graph(VirtualNodeTree &vtree);

}  // namespace DataFlowNodes
}  // namespace FN
