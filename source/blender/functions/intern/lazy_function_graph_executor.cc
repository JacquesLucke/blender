/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_lazy_function_graph_executor.hh"

namespace blender::fn {

struct Executor {
  const LazyFunctionGraph *graph_;
  Span<const LFSocket *> inputs_;
  Span<const LFSocket *> outputs_;
  LazyFunctionParams *params_ = nullptr;

  void execute(LazyFunctionParams &params)
  {
    params_ = &params;
    params_ = nullptr;
  }
};

LazyFunctionGraphExecutor::LazyFunctionGraphExecutor(const LazyFunctionGraph &graph,
                                                     Vector<const LFSocket *> inputs,
                                                     Vector<const LFSocket *> outputs)
    : graph_(graph), inputs_(std::move(inputs)), outputs_(std::move(outputs))
{
}

void LazyFunctionGraphExecutor::execute_impl(LazyFunctionParams &params) const
{
  Executor &executor = params.storage<Executor>();
  executor.execute(params);
}

void *LazyFunctionGraphExecutor::init_storage(LinearAllocator<> &allocator) const
{
  Executor &executor = *allocator.construct<Executor>().release();
  executor.graph_ = &graph_;
  executor.inputs_ = inputs_;
  executor.outputs_ = outputs_;
  return &executor;
}

void LazyFunctionGraphExecutor::destruct_storage(void *storage) const
{
  std::destroy_at(static_cast<Executor *>(storage));
}

}  // namespace blender::fn
