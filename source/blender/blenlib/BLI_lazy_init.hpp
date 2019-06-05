#pragma once

/* These macros help to define functions that initialize
 * some data the first time it is used. */

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
