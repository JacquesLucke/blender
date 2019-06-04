#pragma once

/* These macros help to define functions that initialize
 * some data the first time it is used. */

#include <functional>
#include <BLI_optional.hpp>

namespace BLI {

void register_lazy_init_free_func(std::function<void()> free_func);

}  // namespace BLI

#define LAZY_INIT__NO_ARG(final_ret_type, builder_ret_type, func_name) \
  static builder_ret_type func_name##_impl(void); \
  static builder_ret_type &func_name##_builder(void) \
  { \
    static BLI::Optional<builder_ret_type> value = func_name##_impl(); \
    BLI::register_lazy_init_free_func([]() { value.reset(); }); \
    return value.value(); \
  } \
  final_ret_type func_name(void) \
  { \
    static builder_ret_type &value = func_name##_builder(); \
    return value; \
  } \
  builder_ret_type func_name##_impl(void)

#define LAZY_INIT_STATIC__NO_ARG(final_ret_type, builder_ret_type, func_name) \
  static final_ret_type func_name(void); \
  LAZY_INIT__NO_ARG(final_ret_type, builder_ret_type, func_name)

#define LAZY_INIT_REF__NO_ARG(type, func_name) LAZY_INIT__NO_ARG(type &, type, func_name)

#define LAZY_INIT_REF_STATIC__NO_ARG(type, func_name) \
  LAZY_INIT_STATIC__NO_ARG(type &, type, func_name)
