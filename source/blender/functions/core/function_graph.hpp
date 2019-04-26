#pragma once

#include "data_flow_graph.hpp"

namespace FN {

using DFGraphSocketVector = SmallSetVector<DFGraphSocket>;

class FunctionGraph {
 private:
  SharedDataFlowGraph m_graph;
  DFGraphSocketVector m_inputs;
  DFGraphSocketVector m_outputs;

 public:
  FunctionGraph(SharedDataFlowGraph graph, DFGraphSocketVector inputs, DFGraphSocketVector outputs)
      : m_graph(std::move(graph)), m_inputs(std::move(inputs)), m_outputs(std::move(outputs))
  {
  }

  const SharedDataFlowGraph &graph() const
  {
    return m_graph;
  }

  const DFGraphSocketVector &inputs() const
  {
    return m_inputs;
  }

  const DFGraphSocketVector &outputs() const
  {
    return m_outputs;
  }

  Signature signature() const;
  SmallSet<DFGraphSocket> find_used_sockets(bool include_inputs, bool include_outputs) const;
};

}  // namespace FN
