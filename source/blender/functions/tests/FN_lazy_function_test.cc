/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "FN_lazy_function_execute.hh"
#include "FN_lazy_function_graph.hh"
#include "FN_lazy_function_graph_executor.hh"

#include "BLI_task.h"
#include "BLI_timeit.hh"

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
    const int a = params.get_input<int>(0);
    const int b = params.get_input<int>(1);
    params.set_output(0, a + b);
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
      fn, storage, nullptr, inputs, outputs, input_usages, output_usages, set_outputs);
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

static Vector<LFNode *> build_add_node_chain(LazyFunctionGraph &graph,
                                             const int chain_length,
                                             const int *default_value)
{
  static AddLazyFunction fn;
  Vector<LFNode *> nodes;
  for ([[maybe_unused]] const int i : IndexRange(chain_length)) {
    LFNode &node = graph.add_function(fn);
    node.input(0).set_default_value(default_value);
    node.input(1).set_default_value(default_value);
    nodes.append(&node);
  }
  for (const int i : IndexRange(chain_length - 1)) {
    LFNode &n1 = *nodes[i];
    LFNode &n2 = *nodes[i + 1];
    graph.add_link(n1.output(0), n2.input(0));
  }
  return nodes;
}

struct MultiChainResult {
  Vector<LFNode *> first_nodes;
  LFNode *last_node = nullptr;
};

static MultiChainResult build_multiple_chains(LazyFunctionGraph &graph,
                                              const int chain_length,
                                              const int chain_num,
                                              const int *default_value)
{
  static AddLazyFunction fn;
  MultiChainResult result;
  for ([[maybe_unused]] const int i : IndexRange(chain_num)) {
    Vector<LFNode *> chain = build_add_node_chain(graph, chain_length, default_value);
    result.first_nodes.append(chain[0]);
    if (result.last_node == nullptr) {
      result.last_node = chain.last();
    }
    else {
      LFNode &node = graph.add_function(fn);
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
  LazyFunctionGraph graph;
  MultiChainResult node_chain = build_multiple_chains(graph, 1e4, 24, &value_1);
  LFDummyNode &output_node = graph.add_dummy({&CPPType::get<int>()}, {});
  graph.add_link(node_chain.last_node->output(0), output_node.input(0));
  graph.update_node_indices();
  // std::cout << graph.to_dot() << "\n";

  LazyFunctionGraphExecutor executor_fn{graph, {}, {&output_node.input(0)}};

  // SCOPED_TIMER("run");
  int result;

  for ([[maybe_unused]] const int i : IndexRange(1e2)) {
    execute_lazy_function_test(executor_fn,
                               {LazyFunctionEvent{LazyFunctionEventType::RequestOutput, 0}},
                               Span<GMutablePointer>{{&result}});
  }
  std::cout << "Result: " << result << "\n";
}

}  // namespace blender::fn::tests
