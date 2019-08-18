#pragma once

/**
 * A function graph is a data flow graph with specified inputs and outputs. Therefore, it can be
 * used to define new functions. Multiple function graphs can be build on top of the same data flow
 * graph.
 */

#include "BLI_set_vector.hpp"
#include "BLI_set.hpp"

#include "data_graph.hpp"

namespace FN {

using BLI::SetVector;

class FunctionGraph {
 private:
  SharedDataGraph m_graph;
  SetVector<DataSocket> m_inputs;
  SetVector<DataSocket> m_outputs;

 public:
  FunctionGraph(SharedDataGraph graph, SetVector<DataSocket> inputs, SetVector<DataSocket> outputs)
      : m_graph(std::move(graph)), m_inputs(std::move(inputs)), m_outputs(std::move(outputs))
  {
  }

  const SharedDataGraph &graph() const
  {
    return m_graph;
  }

  SharedDataGraph &graph()
  {
    return m_graph;
  }

  const SetVector<DataSocket> &inputs() const
  {
    return m_inputs;
  }

  const SetVector<DataSocket> &outputs() const
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
  Set<DataSocket> find_used_sockets(bool include_inputs, bool include_outputs) const;
};

}  // namespace FN
