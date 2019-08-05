#pragma once

#include "graph_generation.hpp"

namespace FN {
namespace DataFlowNodes {

class SeparateNodeInputs : public UnlinkedInputGrouper {
 public:
  void group(VTreeDataGraphBuilder &builder, MultiVector<VirtualSocket *> &r_groups) override;
};

class SeparateSocketInputs : public UnlinkedInputGrouper {
 public:
  void group(VTreeDataGraphBuilder &builder, MultiVector<VirtualSocket *> &r_groups) override;
};

class AllInOneSocketInputs : public UnlinkedInputGrouper {
 public:
  void group(VTreeDataGraphBuilder &builder, MultiVector<VirtualSocket *> &r_groups) override;
};

class GroupByNodeUsage : public UnlinkedInputGrouper {
 public:
  void group(VTreeDataGraphBuilder &builder, MultiVector<VirtualSocket *> &r_groups) override;
};

class GroupBySocketUsage : public UnlinkedInputGrouper {
 public:
  void group(VTreeDataGraphBuilder &builder, MultiVector<VirtualSocket *> &r_groups) override;
};

}  // namespace DataFlowNodes
}  // namespace FN
