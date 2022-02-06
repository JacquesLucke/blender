/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "FN_sgraph.hh"

namespace blender::fn::tests {

TEST(node_graph, ToDot)
{
  node_graph_example::ExampleNodeGraphAdapter graph;
  std::cout << node_graph_to_dot(graph) << "\n";
}

}  // namespace blender::fn::tests
