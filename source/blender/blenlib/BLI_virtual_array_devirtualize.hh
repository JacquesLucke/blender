/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_devirtualize_parameters.hh"
#include "BLI_virtual_array.hh"

namespace blender {

/**
 * Generate multiple versions of the given function optimized for different virtual arrays.
 * One has to be careful with nesting multiple devirtualizations, because that results in an
 * exponential number of function instantiations (increasing compile time and binary size).
 *
 * Generally, this function should only be used when the virtual method call overhead to get an
 * element from a virtual array is significant.
 */
template<typename T, typename Func>
inline void devirtualize_varray(const VArray<T> &varray, const Func &func, bool enable = true)
{
  using namespace devirtualize_parameters;
  if (enable) {
    Devirtualizer<decltype(func), VArray<T>> devirtualizer(func, &varray);
    devirtualizer.try_execute_devirtualized(TypeSequence<DispatchVArray<true, true>>());
    if (devirtualizer.executed()) {
      return;
    }
  }
  func(varray);
}

/**
 * Same as `devirtualize_varray`, but devirtualizes two virtual arrays at the same time.
 * This is better than nesting two calls to `devirtualize_varray`, because it instantiates fewer
 * cases.
 */
template<typename T1, typename T2, typename Func>
inline void devirtualize_varray2(const VArray<T1> &varray1,
                                 const VArray<T2> &varray2,
                                 const Func &func,
                                 bool enable = true)
{
  using namespace devirtualize_parameters;
  if (enable) {
    Devirtualizer<decltype(func), VArray<T1>, VArray<T2>> devirtualizer(func, &varray1, &varray2);
    devirtualizer.try_execute_devirtualized(
        TypeSequence<DispatchVArray<true, true>, DispatchVArray<true, true>>());
    if (devirtualizer.executed()) {
      return;
    }
  }
  func(varray1, varray2);
}

}  // namespace blender
