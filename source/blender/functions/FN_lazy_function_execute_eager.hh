/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "BLI_parameter_pack_utils.hh"

#include "FN_lazy_function.hh"

namespace blender::fn {

void execute_lazy_function_eagerly(const LazyFunction &fn,
                                   Span<GMutablePointer> inputs,
                                   Span<GMutablePointer> outputs);

namespace detail {

template<typename... Inputs, typename... Outputs, size_t... InIndices, size_t... OutIndices>
inline void execute_lazy_function_eagerly_impl(
    const LazyFunction &fn,
    std::tuple<Inputs...> &inputs,
    std::tuple<Outputs *...> &outputs,
    std::index_sequence<InIndices...> /* in_indices */,
    std::index_sequence<OutIndices...> /* out_indices */)
{
  Vector<GMutablePointer, 16> input_pointers;
  Vector<GMutablePointer, 16> output_pointers;
  (
      [&]() {
        constexpr size_t I = InIndices;
        using T = Inputs;
        const CPPType &type = CPPType::get<T>();
        input_pointers.append({type, &std::get<I>(inputs)});
      }(),
      ...);
  (
      [&]() {
        constexpr size_t I = OutIndices;
        using T = Outputs;
        const CPPType &type = CPPType::get<T>();
        output_pointers.append({type, std::get<I>(outputs)});
      }(),
      ...);
  execute_lazy_function_eagerly(fn, input_pointers, output_pointers);
}

}  // namespace detail

template<typename... Inputs, typename... Outputs>
inline void execute_lazy_function_eagerly(const LazyFunction &fn,
                                          std::tuple<Inputs...> inputs,
                                          std::tuple<Outputs *...> outputs)
{
  detail::execute_lazy_function_eagerly_impl(fn,
                                             inputs,
                                             outputs,
                                             std::make_index_sequence<sizeof...(Inputs)>(),
                                             std::make_index_sequence<sizeof...(Outputs)>());
}

}  // namespace blender::fn
