#pragma once

/* These macros help to define functions that should only be
 * executed once to initialize some data. The initialized data
 * will only be freed when Blender quits.
 *
 * Requirements:
 *   - Very simple usage, without exposing the implementation details.
 *   - No additional heap allocation for every lazy initialized piece of data.
 *   - Blender has to be able to free all lazy-initialized data shortly
 *     before it quits. This is to make Blenders leak detection not detect
 *     false positives. It would, when we would just depend on static variables.
 *     These are destructed, after Blender prints non-freed memory blocks.
 */

#include <functional>
#include <BLI_optional.hpp>

#include "BLI_lazy_init.h"

namespace BLI {

void lazy_init_register(std::function<void()> free_func, const char *name);

}  // namespace BLI

#define BLI_LAZY_INIT(type, func_name) \
  static type func_name##_impl(void); \
  static type &func_name##_builder(void) \
  { \
    static BLI::Optional<type> value = func_name##_impl(); \
    BLI::lazy_init_register([]() { value.reset(); }, #func_name); \
    return value.value(); \
  } \
  type &func_name(void) \
  { \
    static type &value = func_name##_builder(); \
    return value; \
  } \
  type func_name##_impl(void)

#define BLI_LAZY_INIT_STATIC(type, func_name) \
  static type &func_name(void); \
  BLI_LAZY_INIT(type, func_name)
