/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "BLI_vector_set.hh"

#include "FN_lazy_function_graph.hh"

namespace blender::fn {

class LazyFunctionGraphExecutor : public LazyFunction {
 private:
  const LazyFunctionGraph &graph_;
  VectorSet<const LFOutputSocket *> graph_inputs_;
  VectorSet<const LFInputSocket *> graph_outputs_;

 public:
  LazyFunctionGraphExecutor(const LazyFunctionGraph &graph,
                            Span<const LFOutputSocket *> graph_inputs,
                            Span<const LFInputSocket *> graph_outputs);

  void *init_storage(LinearAllocator<> &allocator) const override;
  void destruct_storage(void *storage) const override;

 private:
  void execute_impl(LFParams &params) const override;
};

}  // namespace blender::fn
