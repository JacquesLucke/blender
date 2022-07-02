/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "BLI_function_ref.hh"
#include "BLI_math_base_safe.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.hh"
#include "BLI_string_ref.hh"

#include "FN_multi_function_builder.hh"

int escape();

#define BLI_MAKE_PROFILE_MARKER(name) \
  struct name##_struct { \
    BLI_NOINLINE inline int operator()(const blender::FunctionRef<void()> f) const \
    { \
      f(); \
      /* Return a value to avoid tail call optimization. */ \
      return escape() + 1; \
    } \
  }; \
  static inline name##_struct name

namespace blender::nodes {

struct FloatMathOperationInfo {
  StringRefNull title_case_name;
  StringRefNull shader_name;

  FloatMathOperationInfo() = delete;
  FloatMathOperationInfo(StringRefNull title_case_name, StringRefNull shader_name)
      : title_case_name(title_case_name), shader_name(shader_name)
  {
  }
};

const FloatMathOperationInfo *get_float_math_operation_info(int operation);
const FloatMathOperationInfo *get_float3_math_operation_info(int operation);
const FloatMathOperationInfo *get_float_compare_operation_info(int operation);

BLI_MAKE_PROFILE_MARKER(MARK_expr);
BLI_MAKE_PROFILE_MARKER(MARK_sqrt);
BLI_MAKE_PROFILE_MARKER(MARK_inv_sqrt);
BLI_MAKE_PROFILE_MARKER(MARK_absolute);
BLI_MAKE_PROFILE_MARKER(MARK_radians);
BLI_MAKE_PROFILE_MARKER(MARK_degrees);
BLI_MAKE_PROFILE_MARKER(MARK_sign);
BLI_MAKE_PROFILE_MARKER(MARK_round);
BLI_MAKE_PROFILE_MARKER(MARK_floor);
BLI_MAKE_PROFILE_MARKER(MARK_ceil);
BLI_MAKE_PROFILE_MARKER(MARK_fraction);
BLI_MAKE_PROFILE_MARKER(MARK_trunc);
BLI_MAKE_PROFILE_MARKER(MARK_sine);
BLI_MAKE_PROFILE_MARKER(MARK_cosine);
BLI_MAKE_PROFILE_MARKER(MARK_tangent);
BLI_MAKE_PROFILE_MARKER(MARK_sinh);
BLI_MAKE_PROFILE_MARKER(MARK_cosh);
BLI_MAKE_PROFILE_MARKER(MARK_tanh);
BLI_MAKE_PROFILE_MARKER(MARK_arcsin);
BLI_MAKE_PROFILE_MARKER(MARK_arccosine);
BLI_MAKE_PROFILE_MARKER(MARK_arctangent);

/**
 * This calls the `callback` with two arguments:
 * 1. The math function that takes a float as input and outputs a new float.
 * 2. A #FloatMathOperationInfo struct reference.
 * Returns true when the callback has been called, otherwise false.
 *
 * The math function that is passed to the callback is actually a lambda function that is different
 * for every operation. Therefore, if the callback is templated on the math function, it will get
 * instantiated for every operation separately. This has two benefits:
 * - The compiler can optimize the callback for every operation separately.
 * - A static variable declared in the callback will be generated for every operation separately.
 *
 * If separate instantiations are not desired, the callback can also take a function pointer with
 * the following signature as input instead: float (*math_function)(float a).
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl_to_fl(const int operation, Callback &&callback)
{
  const FloatMathOperationInfo *info = get_float_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = fn::CustomMF_presets::AllSpanOrSingle();
  static auto exec_preset_slow = fn::CustomMF_presets::Materialized();

  /* This is just an utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function, auto profile_marker_fn) -> bool {
    callback(exec_preset, math_function, *info, profile_marker_fn);
    return true;
  };

  switch (operation) {
    case NODE_MATH_EXPONENT:
      return dispatch(
          exec_preset_slow, [](float a) { return expf(a); }, MARK_expr);
    case NODE_MATH_SQRT:
      return dispatch(
          exec_preset_fast, [](float a) { return safe_sqrtf(a); }, MARK_sqrt);
    case NODE_MATH_INV_SQRT:
      return dispatch(
          exec_preset_fast, [](float a) { return safe_inverse_sqrtf(a); }, MARK_inv_sqrt);
    case NODE_MATH_ABSOLUTE:
      return dispatch(
          exec_preset_fast, [](float a) { return fabs(a); }, MARK_absolute);
    case NODE_MATH_RADIANS:
      return dispatch(
          exec_preset_fast, [](float a) { return (float)DEG2RAD(a); }, MARK_radians);
    case NODE_MATH_DEGREES:
      return dispatch(
          exec_preset_fast, [](float a) { return (float)RAD2DEG(a); }, MARK_degrees);
    case NODE_MATH_SIGN:
      return dispatch(
          exec_preset_fast, [](float a) { return compatible_signf(a); }, MARK_sign);
    case NODE_MATH_ROUND:
      return dispatch(
          exec_preset_fast, [](float a) { return floorf(a + 0.5f); }, MARK_round);
    case NODE_MATH_FLOOR:
      return dispatch(
          exec_preset_fast, [](float a) { return floorf(a); }, MARK_floor);
    case NODE_MATH_CEIL:
      return dispatch(
          exec_preset_fast, [](float a) { return ceilf(a); }, MARK_ceil);
    case NODE_MATH_FRACTION:
      return dispatch(
          exec_preset_fast, [](float a) { return a - floorf(a); }, MARK_fraction);
    case NODE_MATH_TRUNC:
      return dispatch(
          exec_preset_fast, [](float a) { return a >= 0.0f ? floorf(a) : ceilf(a); }, MARK_trunc);
    case NODE_MATH_SINE:
      return dispatch(
          exec_preset_slow, [](float a) { return sinf(a); }, MARK_sine);
    case NODE_MATH_COSINE:
      return dispatch(
          exec_preset_slow, [](float a) { return cosf(a); }, MARK_cosine);
    case NODE_MATH_TANGENT:
      return dispatch(
          exec_preset_slow, [](float a) { return tanf(a); }, MARK_tangent);
    case NODE_MATH_SINH:
      return dispatch(
          exec_preset_slow, [](float a) { return sinhf(a); }, MARK_sinh);
    case NODE_MATH_COSH:
      return dispatch(
          exec_preset_slow, [](float a) { return coshf(a); }, MARK_cosh);
    case NODE_MATH_TANH:
      return dispatch(
          exec_preset_slow, [](float a) { return tanhf(a); }, MARK_tanh);
    case NODE_MATH_ARCSINE:
      return dispatch(
          exec_preset_slow, [](float a) { return safe_asinf(a); }, MARK_arcsin);
    case NODE_MATH_ARCCOSINE:
      return dispatch(
          exec_preset_slow, [](float a) { return safe_acosf(a); }, MARK_arccosine);
    case NODE_MATH_ARCTANGENT:
      return dispatch(
          exec_preset_slow, [](float a) { return atanf(a); }, MARK_arctangent);
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl_fl_to_fl(const int operation, Callback &&callback)
{
  const FloatMathOperationInfo *info = get_float_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = fn::CustomMF_presets::AllSpanOrSingle();
  static auto exec_preset_slow = fn::CustomMF_presets::Materialized();

  /* This is just an utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_MATH_ADD:
      return dispatch(exec_preset_fast, [](float a, float b) { return a + b; });
    case NODE_MATH_SUBTRACT:
      return dispatch(exec_preset_fast, [](float a, float b) { return a - b; });
    case NODE_MATH_MULTIPLY:
      return dispatch(exec_preset_fast, [](float a, float b) { return a * b; });
    case NODE_MATH_DIVIDE:
      return dispatch(exec_preset_fast, [](float a, float b) { return safe_divide(a, b); });
    case NODE_MATH_POWER:
      return dispatch(exec_preset_slow, [](float a, float b) { return safe_powf(a, b); });
    case NODE_MATH_LOGARITHM:
      return dispatch(exec_preset_slow, [](float a, float b) { return safe_logf(a, b); });
    case NODE_MATH_MINIMUM:
      return dispatch(exec_preset_fast, [](float a, float b) { return std::min(a, b); });
    case NODE_MATH_MAXIMUM:
      return dispatch(exec_preset_fast, [](float a, float b) { return std::max(a, b); });
    case NODE_MATH_LESS_THAN:
      return dispatch(exec_preset_fast, [](float a, float b) { return (float)(a < b); });
    case NODE_MATH_GREATER_THAN:
      return dispatch(exec_preset_fast, [](float a, float b) { return (float)(a > b); });
    case NODE_MATH_MODULO:
      return dispatch(exec_preset_fast, [](float a, float b) { return safe_modf(a, b); });
    case NODE_MATH_SNAP:
      return dispatch(exec_preset_fast,
                      [](float a, float b) { return floorf(safe_divide(a, b)) * b; });
    case NODE_MATH_ARCTAN2:
      return dispatch(exec_preset_slow, [](float a, float b) { return atan2f(a, b); });
    case NODE_MATH_PINGPONG:
      return dispatch(exec_preset_fast, [](float a, float b) { return pingpongf(a, b); });
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl_fl_fl_to_fl(const int operation, Callback &&callback)
{
  const FloatMathOperationInfo *info = get_float_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  /* This is just an utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_MATH_MULTIPLY_ADD:
      return dispatch(fn::CustomMF_presets::AllSpanOrSingle(),
                      [](float a, float b, float c) { return a * b + c; });
    case NODE_MATH_COMPARE:
      return dispatch(fn::CustomMF_presets::SomeSpanOrSingle<0, 1>(),
                      [](float a, float b, float c) -> float {
                        return ((a == b) || (fabsf(a - b) <= fmaxf(c, FLT_EPSILON))) ? 1.0f : 0.0f;
                      });
    case NODE_MATH_SMOOTH_MIN:
      return dispatch(fn::CustomMF_presets::SomeSpanOrSingle<0, 1>(),
                      [](float a, float b, float c) { return smoothminf(a, b, c); });
    case NODE_MATH_SMOOTH_MAX:
      return dispatch(fn::CustomMF_presets::SomeSpanOrSingle<0, 1>(),
                      [](float a, float b, float c) { return -smoothminf(-a, -b, c); });
    case NODE_MATH_WRAP:
      return dispatch(fn::CustomMF_presets::SomeSpanOrSingle<0>(),
                      [](float a, float b, float c) { return wrapf(a, b, c); });
  }
  return false;
}

BLI_MAKE_PROFILE_MARKER(MARK_vector_add);
BLI_MAKE_PROFILE_MARKER(MARK_vector_subtract);
BLI_MAKE_PROFILE_MARKER(MARK_vector_multiply);
BLI_MAKE_PROFILE_MARKER(MARK_vector_divide);
BLI_MAKE_PROFILE_MARKER(MARK_vector_cross_product);
BLI_MAKE_PROFILE_MARKER(MARK_vector_project);
BLI_MAKE_PROFILE_MARKER(MARK_vector_reflect);
BLI_MAKE_PROFILE_MARKER(MARK_vector_snap);
BLI_MAKE_PROFILE_MARKER(MARK_vector_modulo);
BLI_MAKE_PROFILE_MARKER(MARK_vector_minimum);
BLI_MAKE_PROFILE_MARKER(MARK_vector_maximum);

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_fl3_to_fl3(const NodeVectorMathOperation operation,
                                                   Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = fn::CustomMF_presets::AllSpanOrSingle();
  static auto exec_preset_slow = fn::CustomMF_presets::Materialized();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function, auto profile_marker_fn) -> bool {
    callback(exec_preset, math_function, *info, profile_marker_fn);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_ADD:
      return dispatch(
          exec_preset_fast, [](float3 a, float3 b) { return a + b; }, MARK_vector_add);
    case NODE_VECTOR_MATH_SUBTRACT:
      return dispatch(
          exec_preset_fast, [](float3 a, float3 b) { return a - b; }, MARK_vector_subtract);
    case NODE_VECTOR_MATH_MULTIPLY:
      return dispatch(
          exec_preset_fast, [](float3 a, float3 b) { return a * b; }, MARK_vector_multiply);
    case NODE_VECTOR_MATH_DIVIDE:
      return dispatch(
          exec_preset_fast,
          [](float3 a, float3 b) { return safe_divide(a, b); },
          MARK_vector_divide);
    case NODE_VECTOR_MATH_CROSS_PRODUCT:
      return dispatch(
          exec_preset_fast,
          [](float3 a, float3 b) { return cross_high_precision(a, b); },
          MARK_vector_cross_product);
    case NODE_VECTOR_MATH_PROJECT:
      return dispatch(
          exec_preset_fast, [](float3 a, float3 b) { return project(a, b); }, MARK_vector_project);
    case NODE_VECTOR_MATH_REFLECT:
      return dispatch(
          exec_preset_fast,
          [](float3 a, float3 b) { return reflect(a, normalize(b)); },
          MARK_vector_reflect);
    case NODE_VECTOR_MATH_SNAP:
      return dispatch(
          exec_preset_fast,
          [](float3 a, float3 b) { return floor(safe_divide(a, b)) * b; },
          MARK_vector_snap);
    case NODE_VECTOR_MATH_MODULO:
      return dispatch(
          exec_preset_slow, [](float3 a, float3 b) { return mod(a, b); }, MARK_vector_modulo);
    case NODE_VECTOR_MATH_MINIMUM:
      return dispatch(
          exec_preset_fast, [](float3 a, float3 b) { return min(a, b); }, MARK_vector_minimum);
    case NODE_VECTOR_MATH_MAXIMUM:
      return dispatch(
          exec_preset_fast, [](float3 a, float3 b) { return max(a, b); }, MARK_vector_maximum);
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_fl3_to_fl(const NodeVectorMathOperation operation,
                                                  Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = fn::CustomMF_presets::AllSpanOrSingle();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_DOT_PRODUCT:
      return dispatch(exec_preset_fast, [](float3 a, float3 b) { return dot(a, b); });
    case NODE_VECTOR_MATH_DISTANCE:
      return dispatch(exec_preset_fast, [](float3 a, float3 b) { return distance(a, b); });
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_fl3_fl3_to_fl3(const NodeVectorMathOperation operation,
                                                       Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = fn::CustomMF_presets::AllSpanOrSingle();
  static auto exec_preset_slow = fn::CustomMF_presets::Materialized();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_MULTIPLY_ADD:
      return dispatch(exec_preset_fast, [](float3 a, float3 b, float3 c) { return a * b + c; });
    case NODE_VECTOR_MATH_WRAP:
      return dispatch(exec_preset_slow, [](float3 a, float3 b, float3 c) {
        return float3(wrapf(a.x, b.x, c.x), wrapf(a.y, b.y, c.y), wrapf(a.z, b.z, c.z));
      });
    case NODE_VECTOR_MATH_FACEFORWARD:
      return dispatch(exec_preset_fast,
                      [](float3 a, float3 b, float3 c) { return faceforward(a, b, c); });
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_fl3_fl_to_fl3(const NodeVectorMathOperation operation,
                                                      Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_slow = fn::CustomMF_presets::Materialized();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_REFRACT:
      return dispatch(exec_preset_slow,
                      [](float3 a, float3 b, float c) { return refract(a, normalize(b), c); });
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_to_fl(const NodeVectorMathOperation operation,
                                              Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = fn::CustomMF_presets::AllSpanOrSingle();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_LENGTH:
      return dispatch(exec_preset_fast, [](float3 in) { return length(in); });
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_fl_to_fl3(const NodeVectorMathOperation operation,
                                                  Callback &&callback)
{
  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = fn::CustomMF_presets::AllSpanOrSingle();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_SCALE:
      return dispatch(exec_preset_fast, [](float3 a, float b) { return a * b; });
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_to_fl3(const NodeVectorMathOperation operation,
                                               Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = fn::CustomMF_presets::AllSpanOrSingle();
  static auto exec_preset_slow = fn::CustomMF_presets::Materialized();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_NORMALIZE:
      return dispatch(exec_preset_fast,
                      [](float3 in) { return normalize(in); }); /* Should be safe. */
    case NODE_VECTOR_MATH_FLOOR:
      return dispatch(exec_preset_fast, [](float3 in) { return floor(in); });
    case NODE_VECTOR_MATH_CEIL:
      return dispatch(exec_preset_fast, [](float3 in) { return ceil(in); });
    case NODE_VECTOR_MATH_FRACTION:
      return dispatch(exec_preset_fast, [](float3 in) { return fract(in); });
    case NODE_VECTOR_MATH_ABSOLUTE:
      return dispatch(exec_preset_fast, [](float3 in) { return abs(in); });
    case NODE_VECTOR_MATH_SINE:
      return dispatch(exec_preset_slow,
                      [](float3 in) { return float3(sinf(in.x), sinf(in.y), sinf(in.z)); });
    case NODE_VECTOR_MATH_COSINE:
      return dispatch(exec_preset_slow,
                      [](float3 in) { return float3(cosf(in.x), cosf(in.y), cosf(in.z)); });
    case NODE_VECTOR_MATH_TANGENT:
      return dispatch(exec_preset_slow,
                      [](float3 in) { return float3(tanf(in.x), tanf(in.y), tanf(in.z)); });
    default:
      return false;
  }
  return false;
}

}  // namespace blender::nodes
