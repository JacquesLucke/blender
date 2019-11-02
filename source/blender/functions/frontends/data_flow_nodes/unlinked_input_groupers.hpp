#pragma once

#include "graph_generation.hpp"

namespace FN {
namespace DataFlowNodes {

class SeparateNodeInputs : public UnlinkedInputsGrouper {
 public:
  void group(VTreeDataGraphBuilder &builder, MultiVector<const VSocket *> &r_groups) override;
};

class SeparateSocketInputs : public UnlinkedInputsGrouper {
 public:
  void group(VTreeDataGraphBuilder &builder, MultiVector<const VSocket *> &r_groups) override;
};

class AllInOneSocketInputs : public UnlinkedInputsGrouper {
 public:
  void group(VTreeDataGraphBuilder &builder, MultiVector<const VSocket *> &r_groups) override;
};

class GroupByNodeUsage : public UnlinkedInputsGrouper {
 public:
  void group(VTreeDataGraphBuilder &builder, MultiVector<const VSocket *> &r_groups) override;
};

class GroupBySocketUsage : public UnlinkedInputsGrouper {
 public:
  void group(VTreeDataGraphBuilder &builder, MultiVector<const VSocket *> &r_groups) override;
};

}  // namespace DataFlowNodes
}  // namespace FN
