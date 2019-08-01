#pragma once

#include "vtree_data_graph_builder.hpp"

namespace FN {
namespace DataFlowNodes {

class UnlinkedInputsHandler {
 public:
  virtual void insert(VTreeDataGraphBuilder &builder,
                      ArrayRef<VirtualSocket *> unlinked_inputs,
                      ArrayRef<BuilderOutputSocket *> r_new_origins) = 0;
};

ValueOrError<VTreeDataGraph> generate_graph(VirtualNodeTree &vtree);

}  // namespace DataFlowNodes
}  // namespace FN
