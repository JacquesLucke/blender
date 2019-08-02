#pragma once

#include "graph_generation.hpp"

namespace FN {
namespace DataFlowNodes {

class SeparateNodeInputs : public UnlinkedInputGrouper {
 public:
  void handle(VTreeDataGraphBuilder &builder, InputInserter &inserter) override;
};

class SeparateSocketInputs : public UnlinkedInputGrouper {
 public:
  void handle(VTreeDataGraphBuilder &builder, InputInserter &inserter) override;
};

class AllInOneSocketInputs : public UnlinkedInputGrouper {
 public:
  void handle(VTreeDataGraphBuilder &builder, InputInserter &inserter) override;
};

class GroupByNodeUsage : public UnlinkedInputGrouper {
 public:
  void handle(VTreeDataGraphBuilder &builder, InputInserter &inserter) override;
};

class GroupBySocketUsage : public UnlinkedInputGrouper {
 public:
  void handle(VTreeDataGraphBuilder &builder, InputInserter &inserter) override;
};

}  // namespace DataFlowNodes
}  // namespace FN
