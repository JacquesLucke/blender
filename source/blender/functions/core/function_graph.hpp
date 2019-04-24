#pragma once

#include "compact_data_flow_graph.hpp"

namespace FN {

using FunctionSocketVector = SmallSetVector<FunctionSocket>;

class CompactFunctionGraph {
 private:
  SharedCompactDataFlowGraph m_graph;
  FunctionSocketVector m_inputs;
  FunctionSocketVector m_outputs;

 public:
  CompactFunctionGraph(SharedCompactDataFlowGraph graph,
                       FunctionSocketVector inputs,
                       FunctionSocketVector outputs)
      : m_graph(std::move(graph)), m_inputs(std::move(inputs)), m_outputs(std::move(outputs))
  {
  }

  const SharedCompactDataFlowGraph &graph() const
  {
    return m_graph;
  }

  const FunctionSocketVector &inputs() const
  {
    return m_inputs;
  }

  const FunctionSocketVector &outputs() const
  {
    return m_outputs;
  }

  Signature signature() const;
  SmallSet<FunctionSocket> find_used_sockets(bool include_inputs, bool include_outputs) const;
};

}  // namespace FN
