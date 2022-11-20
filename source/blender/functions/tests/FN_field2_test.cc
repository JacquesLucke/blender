/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "FN_field2.hh"
#include "FN_lazy_function.hh"
#include "FN_multi_function_builder.hh"

namespace blender::fn::field2::tests {

namespace lf = lazy_function;

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

  void dfg_build(DfgFunctionBuilder &builder) const override
  {
    dfg::Graph &graph = builder.graph();
    dfg::FunctionNode &node = graph.add_function_node(builder.context(), *this, 2, 1, nullptr);
    builder.set_input(0, {&node, 0});
    builder.set_input(1, {&node, 1});
    builder.set_output(0, {&node, 0});
  }

  BackendFlags dfg_node_backends(const void * /*fn_data*/) const override
  {
    return BackendFlags::MultiFunction;
  }

  const MultiFunction &dfg_backend_multi_function(const void * /*fn_data*/,
                                                  ResourceScope & /*scope*/) const override
  {
    static CustomMF_SI_SI_SO<int, int, int> fn{"add", [](int a, int b) { return a + b; }};
    return fn;
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

  void dfg_build(DfgFunctionBuilder &builder) const
  {
    dfg::Graph &graph = builder.graph();

    dfg::FunctionNode &prepare_node = graph.add_function_node(
        builder.context(), *this, 0, 1, reinterpret_cast<void *>(FnData::PrepareContext));
    dfg::FunctionNode &interpolate_node = graph.add_function_node(
        builder.context(), *this, 1, 1, reinterpret_cast<void *>(FnData::Interpolate));

    builder.set_input(0, {&interpolate_node, 1}, {&prepare_node, 0});
    builder.set_output(0, {&interpolate_node, 0});
  }
};

class InputFunc : public FieldFunction {
 private:
  class LazyFuncImpl : public lazy_function::LazyFunction {
   public:
    LazyFuncImpl()
    {
      debug_name_ = "input";
      inputs_.append({"Context", CPPType::get<FieldArrayContextValue>()});
      outputs_.append({"Value", CPPType::get<GVArray>()});
    }

    void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
    {
      const FieldArrayContextValue field_context = params.extract_input<FieldArrayContextValue>(0);
      UNUSED_VARS(field_context);
      params.set_output<GVArray>(0, VArray<int>::ForSingle(4, 10));
    }
  };

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

  void dfg_build(DfgFunctionBuilder &builder) const
  {
    dfg::Graph &graph = builder.graph();

    dfg::FunctionNode &node = graph.add_function_node(builder.context(), *this, 0, 1, nullptr);

    builder.set_output(0, {&node, 0});
  }

  BackendFlags dfg_node_backends(const void * /*fn_data*/) const override
  {
    return BackendFlags::LazyFunction;
  }

  const lazy_function::LazyFunction &dfg_backend_lazy_function(
      const void * /*fn_data*/, ResourceScope & /*scope*/) const override
  {
    static LazyFuncImpl fn;
    return fn;
  }
};

TEST(field, Test)
{
  dfg::Graph graph;
  AddFunc add_func;
  dfg::FunctionNode &add_node1 = graph.add_function_node(
      graph.context_socket(), add_func, 2, 1, nullptr);
  dfg::FunctionNode &add_node2 = graph.add_function_node(
      graph.context_socket(), add_func, 2, 1, nullptr);
  dfg::FunctionNode &add_node3 = graph.add_function_node(
      graph.context_socket(), add_func, 2, 1, nullptr);

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
  // std::cout << "\n\n" << graph.to_dot() << "\n\n";
}

class MyFieldArrayContext : public FieldArrayContext {
 public:
  int64_t array_size() const override
  {
    return 10;
  }
};

TEST(field, Test3)
{
  Field<int> const_42_field = make_constant_field<int>(42);
  Field<int> const_100_field = make_constant_field<int>(100);
  Field<int> input_field1 = std::make_shared<const FieldNode>(std::make_unique<InputFunc>());
  Field<int> input_field2 = std::make_shared<const FieldNode>(std::make_unique<InputFunc>());

  Field<int> added_field = std::make_shared<const FieldNode>(
      std::make_unique<AddFunc>(), Vector<GField>{const_42_field, const_100_field});
  Field<int> added_field2 = std::make_shared<const FieldNode>(
      std::make_unique<AddFunc>(), Vector<GField>{added_field, input_field1});
  Field<int> added_field3 = std::make_shared<const FieldNode>(
      std::make_unique<AddFunc>(), Vector<GField>{added_field, input_field2});
  Field<int> added_field4 = std::make_shared<const FieldNode>(
      std::make_unique<AddFunc>(), Vector<GField>{added_field2, added_field3});

  FieldArrayEvaluator evaluator;
  evaluator.add_field_ref(const_42_field);
  evaluator.add_field_ref(added_field);
  evaluator.add_field_ref(added_field2);
  evaluator.add_field_ref(added_field3);
  evaluator.add_field_ref(added_field4);
  evaluator.finalize();

  MyFieldArrayContext context;
  IndexMask mask = IndexRange(10);
  FieldArrayEvaluation evaluation{evaluator, context, &mask};
  evaluation.evaluate();

  VArray<int> result0 = evaluation.get_evaluated(0).typed<int>();
  VArray<int> result1 = evaluation.get_evaluated(1).typed<int>();
  std::cout << result0.size() << " " << result0[2] << "\n";
  std::cout << result1.size() << " " << result1[2] << "\n";
}

}  // namespace blender::fn::field2::tests
