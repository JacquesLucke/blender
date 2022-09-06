/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "FN_lazy_function_execute.hh"
#include "FN_lazy_function_graph.hh"
#include "FN_lazy_function_graph_executor.hh"

#include "BLI_task.h"
#include "BLI_timeit.hh"

namespace blender::fn::lazy_function::tests {

class AddLazyFunction : public LazyFunction {
 public:
  AddLazyFunction()
  {
    static_name_ = "Add";
    inputs_.append({"A", CPPType::get<int>()});
    inputs_.append({"B", CPPType::get<int>()});
    outputs_.append({"Result", CPPType::get<int>()});
  }

  void execute_impl(Params &params, const Context &UNUSED(context)) const override
  {
    const int a = params.get_input<int>(0);
    const int b = params.get_input<int>(1);
    params.set_output(0, a + b);
  }
};

class StoreValueFunction : public LazyFunction {
 private:
  int *dst1_;
  int *dst2_;

 public:
  StoreValueFunction(int *dst1, int *dst2) : dst1_(dst1), dst2_(dst2)
  {
    static_name_ = "Store Value";
    inputs_.append({"A", CPPType::get<int>()});
    inputs_.append({"B", CPPType::get<int>(), ValueUsage::Maybe});
  }

  void execute_impl(Params &params, const Context &UNUSED(context)) const override
  {
    *dst1_ = params.get_input<int>(0);
    if (int *value = params.try_get_input_data_ptr_or_request<int>(1)) {
      *dst2_ = *value;
    }
  }
};

class SimpleSideEffectProvider : public GraphExecutor::SideEffectProvider {
 private:
  Vector<const FunctionNode *> side_effect_nodes_;

 public:
  SimpleSideEffectProvider(Span<const FunctionNode *> side_effect_nodes)
      : side_effect_nodes_(side_effect_nodes)
  {
  }

  Vector<const FunctionNode *> get_nodes_with_side_effects(
      const Context &UNUSED(context)) const override
  {
    return side_effect_nodes_;
  }
};

TEST(lazy_function, SideEffects)
{
  BLI_task_scheduler_init();
  int dst1 = 0;
  int dst2 = 0;

  const AddLazyFunction add_fn;
  const StoreValueFunction store_fn{&dst1, &dst2};

  Graph graph;
  FunctionNode &add_node_1 = graph.add_function(add_fn);
  FunctionNode &add_node_2 = graph.add_function(add_fn);
  FunctionNode &store_node = graph.add_function(store_fn);
  DummyNode &input_node = graph.add_dummy({}, {&CPPType::get<int>()});

  graph.add_link(input_node.output(0), add_node_1.input(0));
  graph.add_link(input_node.output(0), add_node_2.input(0));
  graph.add_link(add_node_1.output(0), store_node.input(0));
  graph.add_link(add_node_2.output(0), store_node.input(1));

  const int value_10 = 10;
  const int value_100 = 100;
  add_node_1.input(1).set_default_value(&value_10);
  add_node_2.input(1).set_default_value(&value_100);

  graph.update_node_indices();

  SimpleSideEffectProvider side_effect_provider{{&store_node}};

  GraphExecutor executor_fn{graph, {&input_node.output(0)}, {}, nullptr, &side_effect_provider};
  execute_lazy_function_eagerly(executor_fn, nullptr, std::make_tuple(5), std::make_tuple());

  EXPECT_EQ(dst1, 15);
  EXPECT_EQ(dst2, 105);
}

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
  const Span<Input> fn_inputs = fn.inputs();
  const Span<Output> fn_outputs = fn.outputs();
  BLI_assert(outputs.size() == fn_outputs.size());

  LinearAllocator<> allocator;
  Vector<GMutablePointer> inputs(fn_inputs.size());
  Array<std::optional<ValueUsage>> input_usages(fn_inputs.size());
  Array<ValueUsage> output_usages(fn_outputs.size(), ValueUsage::Unused);
  Array<bool> set_outputs(fn_outputs.size(), false);

  void *storage = fn.init_storage(allocator);
  Context context;
  context.storage = storage;

  BasicParams params(fn, inputs, outputs, input_usages, output_usages, set_outputs);
  if (fn.always_used_inputs_available(params)) {
    fn.execute(params, context);
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
    if (fn.always_used_inputs_available(params)) {
      fn.execute(params, context);
    }
  }

  fn.destruct_storage(storage);
}

static Vector<Node *> build_add_node_chain(Graph &graph,
                                           const int chain_length,
                                           const int *default_value)
{
  static AddLazyFunction fn;
  Vector<Node *> nodes;
  for ([[maybe_unused]] const int i : IndexRange(chain_length)) {
    Node &node = graph.add_function(fn);
    node.input(0).set_default_value(default_value);
    node.input(1).set_default_value(default_value);
    nodes.append(&node);
  }
  for (const int i : IndexRange(chain_length - 1)) {
    Node &n1 = *nodes[i];
    Node &n2 = *nodes[i + 1];
    graph.add_link(n1.output(0), n2.input(0));
  }
  return nodes;
}

struct MultiChainResult {
  Vector<Node *> first_nodes;
  Node *last_node = nullptr;
};

static MultiChainResult build_multiple_chains(Graph &graph,
                                              const int chain_length,
                                              const int chain_num,
                                              const int *default_value)
{
  static AddLazyFunction fn;
  MultiChainResult result;
  for ([[maybe_unused]] const int i : IndexRange(chain_num)) {
    Vector<Node *> chain = build_add_node_chain(graph, chain_length, default_value);
    result.first_nodes.append(chain[0]);
    if (result.last_node == nullptr) {
      result.last_node = chain.last();
    }
    else {
      Node &node = graph.add_function(fn);
      node.input(0).set_default_value(default_value);
      node.input(1).set_default_value(default_value);
      graph.add_link(result.last_node->output(0), node.input(0));
      graph.add_link(chain.last()->output(0), node.input(1));
      result.last_node = &node;
    }
  }
  return result;
}

TEST(lazy_function, Simple)
{
  BLI_task_scheduler_init(); /* Without this, no parallelism. */
  const int value_1 = 1;
  Graph graph;
  MultiChainResult node_chain = build_multiple_chains(graph, 1e4, 24, &value_1);
  DummyNode &output_node = graph.add_dummy({&CPPType::get<int>()}, {});
  graph.add_link(node_chain.last_node->output(0), output_node.input(0));
  graph.update_node_indices();
  // std::cout << graph.to_dot() << "\n";

  GraphExecutor executor_fn{graph, {}, {&output_node.input(0)}, nullptr, nullptr};

  // SCOPED_TIMER("run");
  int result;

  for ([[maybe_unused]] const int i : IndexRange(1e2)) {
    execute_lazy_function_test(executor_fn,
                               {LazyFunctionEvent{LazyFunctionEventType::RequestOutput, 0}},
                               Span<GMutablePointer>{{&result}});
  }
  std::cout << "Result: " << result << "\n";
}

}  // namespace blender::fn::lazy_function::tests
