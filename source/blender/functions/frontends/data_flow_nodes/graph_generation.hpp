#pragma once

#include "BLI_multi_vector.hpp"

#include "vtree_data_graph_builder.hpp"

namespace FN {
namespace DataFlowNodes {

using BLI::MultiVector;

class UnlinkedInputsGrouper {
 public:
  virtual void group(VTreeDataGraphBuilder &builder, MultiVector<VirtualSocket *> &r_groups) = 0;
};

class UnlinkedInputsInserter {
 public:
  virtual void insert(VTreeDataGraphBuilder &builder,
                      ArrayRef<VirtualSocket *> unlinked_inputs,
                      MutableArrayRef<BuilderOutputSocket *> r_new_origins) = 0;
};

ValueOrError<VTreeDataGraph> generate_graph(VirtualNodeTree &vtree);

ValueOrError<VTreeDataGraph> generate_graph(VirtualNodeTree &vtree,
                                            UnlinkedInputsGrouper &inputs_grouper,
                                            UnlinkedInputsInserter &inputs_inserter);

}  // namespace DataFlowNodes
}  // namespace FN
