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

enum class LazyFunctionEventType {
  SetInput,
  RequestOutput,
  SetOutputUnused,
};

struct LazyFunctionEvent {
  LazyFunctionEventType type;
  int index;
  void *value;
};

static void execute_lazy_function_test(const LazyFunction &fn,
                                       const Span<LazyFunctionEvent> events,
                                       const Span<GMutablePointer> outputs)
{
  const Span<LazyFunctionInput> fn_inputs = fn.inputs();
  const Span<LazyFunctionOutput> fn_outputs = fn.outputs();
  BLI_assert(outputs.size() == fn_outputs.size());

  LinearAllocator<> allocator;
  Vector<GMutablePointer> inputs(fn_inputs.size());
  Array<std::optional<ValueUsage>> input_usages(fn_inputs.size());
  Array<ValueUsage> output_usages(fn_outputs.size(), ValueUsage::Unused);
  Array<bool> set_outputs(fn_outputs.size(), false);

  void *storage = fn.init_storage(allocator);

  BasicLazyFunctionParams params(
      fn, storage, inputs, outputs, input_usages, output_usages, set_outputs);
  if (fn.valid_params_for_execution(params)) {
    fn.execute(params);
  }
  for (const LazyFunctionEvent &event : events) {
    switch (event.type) {
      case LazyFunctionEventType::SetInput: {
        inputs[event.index] = GMutablePointer{fn_inputs[event.index].type, event.value};
        break;
      }
      case LazyFunctionEventType::RequestOutput: {
        output_usages[event.index] = ValueUsage::Used;
        break;
      }
      case LazyFunctionEventType::SetOutputUnused: {
        output_usages[event.index] = ValueUsage::Unused;
        break;
      }
    }
    if (fn.valid_params_for_execution(params)) {
      fn.execute(params);
    }
  }

  fn.destruct_storage(storage);
}

TEST(lazy_function, Simple)
{
  AddLazyFunction fn;

  // {
  //   int result;
  //   execute_lazy_function_eagerly(fn, std::make_tuple<int, int>(3, 6),
  //   std::make_tuple(&result)); std::cout << result << "\n";
  // }

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

  LazyFunctionGraphExecutor executor_fn{graph, {&n1.input(0)}, {&n2.output(0)}};
  // {
  //   int result;
  //   execute_lazy_function_eagerly(executor_fn, std::make_tuple<>(10), std::make_tuple(&result));
  //   std::cout << result << "\n";
  // }

  {
    int value_10 = 10;
    int result;
    execute_lazy_function_test(executor_fn,
                               {LazyFunctionEvent{LazyFunctionEventType::RequestOutput, 0},
                                LazyFunctionEvent{LazyFunctionEventType::SetInput, 0, &value_10}},
                               Span<GMutablePointer>{{&result}});
    std::cout << "Result: " << result << "\n";
  }
}

}  // namespace blender::fn::tests
