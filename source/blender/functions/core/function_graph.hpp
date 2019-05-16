#pragma once

#include "data_flow_graph.hpp"

namespace FN {

using DFGraphSocketSetVector = SmallSetVector<DFGraphSocket>;

class FunctionGraph {
 private:
  SharedDataFlowGraph m_graph;
  DFGraphSocketSetVector m_inputs;
  DFGraphSocketSetVector m_outputs;

 public:
  FunctionGraph(SharedDataFlowGraph graph,
                DFGraphSocketSetVector inputs,
                DFGraphSocketSetVector outputs)
      : m_graph(std::move(graph)), m_inputs(std::move(inputs)), m_outputs(std::move(outputs))
  {
  }

  const SharedDataFlowGraph &graph() const
  {
    return m_graph;
  }

  SharedDataFlowGraph &graph()
  {
    return m_graph;
  }

  const DFGraphSocketSetVector &inputs() const
  {
    return m_inputs;
  }

  const DFGraphSocketSetVector &outputs() const
  {
    return m_outputs;
  }

  SharedFunction new_function(StringRef name) const;
  SmallSet<DFGraphSocket> find_used_sockets(bool include_inputs, bool include_outputs) const;
};

}  // namespace FN
