#pragma once

#include "graph_generation.hpp"

namespace FN {
namespace DataFlowNodes {

class DynamicSocketLoader : public UnlinkedInputsInserter {
 public:
  void insert(VTreeDataGraphBuilder &builder,
              ArrayRef<VirtualSocket *> unlinked_inputs,
              ArrayRef<BuilderOutputSocket *> r_new_origins) override;
};

class ConstantInputsHandler : public UnlinkedInputsInserter {
 public:
  void insert(VTreeDataGraphBuilder &builder,
              ArrayRef<VirtualSocket *> unlinked_inputs,
              ArrayRef<BuilderOutputSocket *> r_new_origins) override;
};

}  // namespace DataFlowNodes
}  // namespace FN
