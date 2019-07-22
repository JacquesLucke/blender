#pragma once

/**
 * A function graph is a data flow graph with specified inputs and outputs. Therefore, it can be
 * used to define new functions. Multiple function graphs can be build on top of the same data flow
 * graph.
 */

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

  /**
   * Create a new function with the given name. The inputs and outputs correspond to the sockets in
   * the graph. The returned function does not contain any bodies.
   */
  SharedFunction new_function(StringRef name) const;

  /**
   * Get a subset of all sockets in the graph that can influence the function execution (under the
   * assumption, that functions do not have side effects).
   */
  Set<DFGraphSocket> find_used_sockets(bool include_inputs, bool include_outputs) const;
};

}  // namespace FN
