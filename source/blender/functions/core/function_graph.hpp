#pragma once

/**
 * A function graph is a data flow graph with specified inputs and outputs. Therefore, it can be
 * used to define new functions. Multiple function graphs can be build on top of the same data flow
 * graph.
 */

#include "BLI_vector_set.h"
#include "BLI_set.h"

#include "data_graph.hpp"

namespace FN {

using BLI::VectorSet;

class FunctionGraph {
 private:
  DataGraph *m_graph;
  VectorSet<DataSocket> m_inputs;
  VectorSet<DataSocket> m_outputs;

 public:
  FunctionGraph(DataGraph &graph, VectorSet<DataSocket> inputs, VectorSet<DataSocket> outputs)
      : m_graph(&graph), m_inputs(std::move(inputs)), m_outputs(std::move(outputs))
  {
  }

  const DataGraph &graph() const
  {
    return *m_graph;
  }

  DataGraph &graph()
  {
    return *m_graph;
  }

  const VectorSet<DataSocket> &inputs() const
  {
    return m_inputs;
  }

  const VectorSet<DataSocket> &outputs() const
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
