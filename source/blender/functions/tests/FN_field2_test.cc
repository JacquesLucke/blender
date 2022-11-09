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

  std::string dfg_node_name(const void * /*fn_data*/) const override
  {
    return "add";
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

class ChangeContextFunc : public FieldFunction {
 private:
  enum class FnData {
    PrepareContext,
    Interpolate,
  };

 public:
  ChangeContextFunc() : FieldFunction(1, 1)
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

  std::string dfg_node_name(const void *fn_data) const override
  {
    switch (FnData(uintptr_t(fn_data))) {
      case FnData::PrepareContext:
        return "prepare context";
      case FnData::Interpolate:
        return "interpolate";
    }
    BLI_assert_unreachable();
    return "";
  }

  int dfg_inputs_num(const void *fn_data) const override
  {
    switch (FnData(uintptr_t(fn_data))) {
      case FnData::PrepareContext:
        return 1;
      case FnData::Interpolate:
        return 2;
    }
    BLI_assert_unreachable();
    return 0;
  }

  int dfg_outputs_num(const void *fn_data) const override
  {
    switch (FnData(uintptr_t(fn_data))) {
      case FnData::PrepareContext:
        return 1;
      case FnData::Interpolate:
        return 1;
    }
    BLI_assert_unreachable();
    return 0;
  }

  void dfg_build(DfgFunctionBuilder &builder) const
  {
    dfg::Graph &graph = builder.graph();

    dfg::FunctionNode &prepare_node = graph.add_function_node(
        *this, reinterpret_cast<void *>(FnData::PrepareContext));
    dfg::FunctionNode &interpolate_node = graph.add_function_node(
        *this, reinterpret_cast<void *>(FnData::Interpolate));

    graph.add_link(builder.context(), {&prepare_node, 0});
    graph.add_link(builder.context(), {&interpolate_node, 0});

    builder.set_input(0, {&interpolate_node, 1}, {&prepare_node, 0});
    builder.set_output(0, {&interpolate_node, 0});
  }
};

class InputFunc : public FieldFunction {
 public:
  InputFunc() : FieldFunction(0, 1)
  {
  }

  const CPPType &output_cpp_type_impl(int /*index*/) const override
  {
    return CPPType::get<int>();
  }

  std::string dfg_node_name(const void * /*fn_data*/) const override
  {
    return "input";
  }

  int dfg_inputs_num(const void * /*fn_data*/) const override
  {
    return 1;
  }

  int dfg_outputs_num(const void * /*fn_data*/) const override
  {
    return 1;
  }

  void dfg_build(DfgFunctionBuilder &builder) const
  {
    dfg::Graph &graph = builder.graph();

    dfg::FunctionNode &node = graph.add_function_node(*this);
    graph.add_link(builder.context(), {&node, 0});

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

  // std::cout << "\n\n" << graph.to_dot() << "\n\n";
}

TEST(field, Test2)
{
  Field<int> const_field = make_constant_field<int>(4);
  Field<int> input_field = std::make_shared<const FieldNode>(std::make_unique<InputFunc>(),
                                                             Vector<GField>{});

  Field<int> field1 = std::make_shared<const FieldNode>(std::make_unique<AddFunc>(),
                                                        Vector<GField>{const_field, input_field});

  Field<int> change_context_field = std::make_shared<const FieldNode>(
      std::make_unique<ChangeContextFunc>(), Vector<GField>{field1});

  dfg::Graph graph;
  build_dfg_for_fields(graph, {field1, const_field, change_context_field});
  std::cout << "\n\n" << graph.to_dot() << "\n\n";
}

}  // namespace blender::fn::field2::tests
