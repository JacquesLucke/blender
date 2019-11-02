#pragma once

#include "BLI_multi_vector.h"
#include "BLI_optional.h"

#include "vtree_data_graph_builder.hpp"

namespace FN {
namespace DataFlowNodes {

using BLI::MultiVector;
using BLI::Optional;

class UnlinkedInputsGrouper {
 public:
  virtual void group(VTreeDataGraphBuilder &builder, MultiVector<const VSocket *> &r_groups) = 0;
};

class UnlinkedInputsInserter {
 public:
  virtual void insert(VTreeDataGraphBuilder &builder,
                      ArrayRef<const VSocket *> unlinked_inputs,
                      MutableArrayRef<BuilderOutputSocket *> r_new_origins) = 0;
};

Optional<std::unique_ptr<VTreeDataGraph>> generate_graph(VirtualNodeTree &vtree);

Optional<std::unique_ptr<VTreeDataGraph>> generate_graph(VirtualNodeTree &vtree,
                                                         UnlinkedInputsGrouper &inputs_grouper,
                                                         UnlinkedInputsInserter &inputs_inserter);

}  // namespace DataFlowNodes
}  // namespace FN
