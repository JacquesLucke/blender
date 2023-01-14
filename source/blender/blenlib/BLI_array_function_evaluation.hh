/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"

namespace blender::array_function_evaluation {

/**
 * Executes #element_fn for all indices in the mask with the arguments at that index.
 */
template<typename MaskT, typename... Args, typename ElementFn>
/* Perform additional optimizations on this loop because it is a very hot loop. For example, the
 * math node in geometry nodes is processed here.  */
#if (defined(__GNUC__) && !defined(__clang__))
[[gnu::optimize("-funroll-loops")]] [[gnu::optimize("O3")]]
#endif
inline void
execute_array(ElementFn element_fn,
              MaskT mask,
              /* Use restrict to tell the compiler that pointer inputs do not alias each
               * other. This is important for some compiler optimizations. */
              Args &&__restrict... args)
{
  if constexpr (std::is_integral_v<MaskT>) {
    /* Having this explicit loop is necessary for MSVC to be able to vectorize this. */
    const int64_t end = int64_t(mask);
    for (int64_t i = 0; i < end; i++) {
      element_fn(args[i]...);
    }
  }
  else if constexpr (std::is_same_v<std::decay_t<MaskT>, IndexRange>) {
    /* Having this explicit loop is necessary for MSVC to be able to vectorize this. */
    const int64_t start = mask.start();
    const int64_t end = mask.one_after_last();
    for (int64_t i = start; i < end; i++) {
      element_fn(args[i]...);
    }
  }
  else {
    for (const int64_t i : mask) {
      element_fn(args[i]...);
    }
  }
}

}  // namespace blender::array_function_evaluation
