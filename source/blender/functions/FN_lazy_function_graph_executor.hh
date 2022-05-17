/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "FN_lazy_function_graph.hh"

namespace blender::fn {

class LazyFunctionGraphExecutor : public LazyFunction {
 private:
  const LazyFunctionGraph &graph_;
  Vector<const LFSocket *> inputs_;
  Vector<const LFSocket *> outputs_;

 public:
  LazyFunctionGraphExecutor(const LazyFunctionGraph &graph,
                            Vector<const LFSocket *> inputs,
                            Vector<const LFSocket *> outputs);

  void execute_impl(LazyFunctionParams &params) const override;
};

}  // namespace blender::fn
