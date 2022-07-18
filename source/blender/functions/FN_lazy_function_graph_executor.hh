/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "BLI_vector_set.hh"

#include "FN_lazy_function_graph.hh"

namespace blender::fn::lazy_function {

class LazyFunctionGraphExecutor : public LazyFunction {
 private:
  const LazyFunctionGraph &graph_;
  VectorSet<const OutputSocket *> graph_inputs_;
  VectorSet<const InputSocket *> graph_outputs_;

 public:
  LazyFunctionGraphExecutor(const LazyFunctionGraph &graph,
                            Span<const OutputSocket *> graph_inputs,
                            Span<const InputSocket *> graph_outputs);

  void *init_storage(LinearAllocator<> &allocator) const override;
  void destruct_storage(void *storage) const override;

 private:
  void execute_impl(Params &params, const Context &context) const override;
};

}  // namespace blender::fn::lazy_function
