/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "FN_lazy_function_execute_eager.hh"
#include "FN_lazy_function_graph.hh"
#include "FN_lazy_function_graph_executor.hh"

namespace blender::fn::tests {

class AddLazyFunction : public LazyFunction {
 public:
  AddLazyFunction()
  {
    static_name_ = "Add";
    inputs_.append({"A", CPPType::get<int>()});
    inputs_.append({"B", CPPType::get<int>()});
    outputs_.append({"Result", CPPType::get<int>()});
  }

  void execute_impl(LazyFunctionParams &params) const override
  {
    const int a = params.get_input<int>(0, "A");
    const int b = params.get_input<int>(1, "B");
    params.set_output(0, a + b, "Result");
  }
};

TEST(lazy_function, Simple)
{
  AddLazyFunction fn;

  // {
  //   int result;
  //   execute_lazy_function_eagerly(fn, std::make_tuple<int, int>(3, 6),
  //   std::make_tuple(&result)); std::cout << result << "\n";
  // }

  const int value_1 = 1;
  const int value_2 = 2;
  const int value_5 = 5;

  LazyFunctionGraph graph;
  LFNode &n1 = graph.add_node(fn);
  // n1.input(0).set_default_value(&value_1);
  n1.input(1).set_default_value(&value_2);
  LFNode &n2 = graph.add_node(fn);
  n2.input(0).set_default_value(&value_5);
  graph.add_link(*n1.outputs()[0], *n2.inputs()[1]);
  std::cout << graph.to_dot() << "\n";

  LazyFunctionGraphExecutor executor{graph, {&n1.input(0)}, {&n2.output(0)}};
  {
    int result;
    execute_lazy_function_eagerly(executor, std::make_tuple<>(10), std::make_tuple(&result));
    std::cout << result << "\n";
  }
}

}  // namespace blender::fn::tests
