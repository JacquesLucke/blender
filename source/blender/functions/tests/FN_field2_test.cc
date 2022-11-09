/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "FN_field2.hh"

namespace blender::fn::field2::tests {

class AddFunc : public FieldFunction {
 public:
  AddFunc() : FieldFunction(2, 1)
  {
  }

  const CPPType &input_cpp_type_impl(int /*index*/) const override
  {
    return CPPType::get<int>();
  }

  const CPPType &output_cpp_type_impl(int /*index*/) const override
  {
    return CPPType::get<int>();
  }

  int dfg_inputs_num(const void * /*fn_data*/) const override
  {
    return 2;
  }

  int dfg_outputs_num(const void * /*fn_data*/) const override
  {
    return 1;
  }

  void dfg_build(DfgFunctionBuilder &builder) const
  {
    dfg::Graph &graph = builder.graph();
    dfg::FunctionNode &node = graph.add_function_node(*this);
    builder.set_input(0, {&node, 0});
    builder.set_input(1, {&node, 1});
    builder.set_output(0, {&node, 0});
  }
};

TEST(field, Test)
{
  dfg::Graph graph;
  AddFunc add_func;
  dfg::FunctionNode &add_node1 = graph.add_function_node(add_func, nullptr);
  dfg::FunctionNode &add_node2 = graph.add_function_node(add_func, nullptr);
  dfg::FunctionNode &add_node3 = graph.add_function_node(add_func, nullptr);

  graph.add_link({&add_node1, 0}, {&add_node2, 0});
  graph.add_link({&add_node1, 0}, {&add_node2, 1});
  graph.add_link({&add_node2, 0}, {&add_node3, 1});

  std::cout << "\n\n" << graph.to_dot() << "\n\n";
}

TEST(field, Test2)
{
  Field<int> field = std::make_shared<const FieldNode>(
      std::make_unique<AddFunc>(),
      Vector<GField>{make_constant_field<int>(4), make_constant_field<int>(10)});
}

}  // namespace blender::fn::field2::tests
