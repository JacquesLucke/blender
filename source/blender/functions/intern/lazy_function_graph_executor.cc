/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_lazy_function_graph_executor.hh"

namespace blender::fn {

LazyFunctionGraphExecutor::LazyFunctionGraphExecutor(const LazyFunctionGraph &graph,
                                                     Vector<const LFSocket *> inputs,
                                                     Vector<const LFSocket *> outputs)
    : graph_(graph), inputs_(std::move(inputs)), outputs_(std::move(outputs))
{
}

void LazyFunctionGraphExecutor::execute_impl(LazyFunctionParams &params) const
{
}

}  // namespace blender::fn
