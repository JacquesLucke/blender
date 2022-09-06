/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "FN_lazy_function_graph.hh"

namespace blender::fn::lazy_function {

class LazyFunctionGraphExecutionLogger {
 public:
  virtual void log_socket_value(const Context &context,
                                const Socket &socket,
                                GPointer value) const;
};

class LazyFunctionGraphExecutionSideEffectProvider {
 public:
  virtual Vector<const Node *> get_nodes_with_side_effects(const Context &context) const;
};

class LazyFunctionGraphExecutor : public LazyFunction {
 public:
  using Logger = LazyFunctionGraphExecutionLogger;
  using SideEffectProvider = LazyFunctionGraphExecutionSideEffectProvider;

 private:
  /**
   * The graph that is evaluated.
   */
  const LazyFunctionGraph &graph_;
  /**
   * Input and output sockets of the entire graph.
   */
  VectorSet<const OutputSocket *> graph_inputs_;
  VectorSet<const InputSocket *> graph_outputs_;
  /**
   * Optional logger for events that happen during execution.
   */
  const Logger *logger_;
  /**
   * Optional side effect provider. It knows which nodes have side effects based on the context
   * during evaluation.
   */
  const SideEffectProvider *side_effect_provider_;

  friend class Executor;

 public:
  LazyFunctionGraphExecutor(const LazyFunctionGraph &graph,
                            Span<const OutputSocket *> graph_inputs,
                            Span<const InputSocket *> graph_outputs,
                            const Logger *logger,
                            const SideEffectProvider *side_effect_provider);

  void *init_storage(LinearAllocator<> &allocator) const override;
  void destruct_storage(void *storage) const override;

 private:
  void execute_impl(Params &params, const Context &context) const override;
};

}  // namespace blender::fn::lazy_function
