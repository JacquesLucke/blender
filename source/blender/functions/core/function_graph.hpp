#pragma once

#include "compact_data_flow_graph.hpp"

namespace FN {

using FunctionSockets = SmallSetVector<FunctionSocket>;

class CompactFunctionGraph {
 private:
  SharedCompactDataFlowGraph m_graph;
  FunctionSockets m_inputs;
  FunctionSockets m_outputs;

 public:
  SharedCompactDataFlowGraph &graph()
  {
    return m_graph;
  }

  const FunctionSockets &inputs() const
  {
    return m_inputs;
  }

  const FunctionSockets &outputs() const
  {
    return m_outputs;
  }

  Signature CompactFunctionGraph::signature() const;
  SmallSet<FunctionSocket> find_used_sockets(bool include_inputs, bool include_outputs) const;
};

}  // namespace FN
