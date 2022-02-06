/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "FN_sgraph.hh"

namespace blender::fn::sgraph::tests {

struct ExampleSGraphAdapter {
  using NodeID = int;

  int node_inputs_size(const NodeID &node) const
  {
    switch (node) {
      case 1:
        return 2;
      case 2:
        return 2;
      case 3:
        return 3;
    }
    BLI_assert_unreachable();
    return 0;
  }

  int node_outputs_size(const NodeID &node) const
  {
    switch (node) {
      case 1:
        return 2;
      case 2:
        return 1;
      case 3:
        return 3;
    }
    BLI_assert_unreachable();
    return 0;
  }

  template<typename F> void foreach_node(const F &f) const
  {
    f(1);
    f(2);
    f(3);
  }

  template<typename F>
  void foreach_linked_input(const NodeID &node, const int output_socket_index, const F &f) const
  {
    switch (node * 1000 + output_socket_index) {
      case 1000:
        f(2, 0);
        break;
      case 1001:
        f(2, 1);
        f(3, 2);
        break;
      case 2000:
        f(3, 0);
        break;
      default:
        break;
    }
  }

  template<typename F>
  void foreach_linked_output(const NodeID &node, const int input_socket_index, const F &f) const
  {
    switch (node * 1000 + input_socket_index) {
      case 2000:
        f(1, 0);
        break;
      case 2001:
        f(1, 1);
        break;
      case 3000:
        f(2, 0);
        break;
      case 3002:
        f(1, 1);
        break;
      default:
        break;
    }
  }

  std::string node_debug_name(const NodeID &node) const
  {
    return std::to_string(node);
  }

  std::string input_socket_debug_name(const NodeID &node, const int input_socket_index) const
  {
    return std::to_string(node * 1000 + input_socket_index);
  }

  std::string output_socket_debug_name(const NodeID &node, const int output_socket_index) const
  {
    return std::to_string(node * 1000 + output_socket_index);
  }
};

TEST(sgraph, ToDot)
{
  ExampleSGraphAdapter adapter;
  SGraph<ExampleSGraphAdapter> graph{adapter};
  std::cout << sgraph_to_dot(graph) << "\n";
}

}  // namespace blender::fn::sgraph::tests
