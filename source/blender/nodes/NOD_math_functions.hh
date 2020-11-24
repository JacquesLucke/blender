/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include "DNA_node_types.h"

#include "BLI_math_base_safe.h"
#include "BLI_math_rotation.h"
#include "BLI_string_ref.hh"

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

const FloatMathOperationInfo *get_float_math_operation_info(const int operation);

template<typename OpType>
inline bool dispatch_float_math_fl_fl_to_fl(const int operation, OpType &&op)
{
  const FloatMathOperationInfo *info = get_float_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  auto dispatch = [&](auto function) -> bool {
    op(function, *info);
    return true;
  };

  switch (operation) {
    case NODE_MATH_ADD:
      return dispatch([](float a, float b) { return a + b; });
    case NODE_MATH_SUBTRACT:
      return dispatch([](float a, float b) { return a - b; });
    case NODE_MATH_MULTIPLY:
      return dispatch([](float a, float b) { return a * b; });
    case NODE_MATH_DIVIDE:
      return dispatch(safe_divide);
    case NODE_MATH_POWER:
      return dispatch(safe_powf);
    case NODE_MATH_LOGARITHM:
      return dispatch(safe_logf);
    case NODE_MATH_MINIMUM:
      return dispatch([](float a, float b) { return std::min(a, b); });
    case NODE_MATH_MAXIMUM:
      return dispatch([](float a, float b) { return std::max(a, b); });
    case NODE_MATH_LESS_THAN:
      return dispatch([](float a, float b) { return (float)(a < b); });
    case NODE_MATH_GREATER_THAN:
      return dispatch([](float a, float b) { return (float)(a > b); });
    case NODE_MATH_MODULO:
      return dispatch(safe_modf);
    case NODE_MATH_SNAP:
      return dispatch([](float a, float b) { return floorf(safe_divide(a, b)) * b; });
    case NODE_MATH_ARCTAN2:
      return dispatch([](float a, float b) { return atan2f(a, b); });
  }
  return false;
}

template<typename OpType>
inline bool dispatch_float_math_fl_to_fl(const int operation, OpType &&op)
{
  const FloatMathOperationInfo *info = get_float_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  auto dispatch = [&](auto function) -> bool {
    op(function, *info);
    return true;
  };

  switch (operation) {
    case NODE_MATH_EXPONENT:
      return dispatch(expf);
    case NODE_MATH_SQRT:
      return dispatch(safe_sqrtf);
    case NODE_MATH_INV_SQRT:
      return dispatch(safe_inverse_sqrtf);
    case NODE_MATH_ABSOLUTE:
      return dispatch([](float a) { return fabs(a); });
    case NODE_MATH_RADIANS:
      return dispatch([](float a) { return DEG2RAD(a); });
    case NODE_MATH_DEGREES:
      return dispatch([](float a) { return RAD2DEG(a); });
    case NODE_MATH_SIGN:
      return dispatch(compatible_signf);
    case NODE_MATH_ROUND:
      return dispatch([](float a) { return floorf(a + 0.5f); });
    case NODE_MATH_FLOOR:
      return dispatch([](float a) { return floorf(a); });
    case NODE_MATH_CEIL:
      return dispatch([](float a) { return ceilf(a); });
    case NODE_MATH_FRACTION:
      return dispatch([](float a) { return a - floorf(a); });
    case NODE_MATH_TRUNC:
      return dispatch([](float a) { return a >= 0.0f ? floorf(a) : ceilf(a); });
    case NODE_MATH_SINE:
      return dispatch([](float a) { return sinf(a); });
    case NODE_MATH_COSINE:
      return dispatch([](float a) { return cosf(a); });
    case NODE_MATH_TANGENT:
      return dispatch([](float a) { return tanf(a); });
    case NODE_MATH_SINH:
      return dispatch([](float a) { return sinhf(a); });
    case NODE_MATH_COSH:
      return dispatch([](float a) { return coshf(a); });
    case NODE_MATH_TANH:
      return dispatch([](float a) { return tanhf(a); });
    case NODE_MATH_ARCSINE:
      return dispatch([](float a) { return safe_asinf(a); });
    case NODE_MATH_ARCCOSINE:
      return dispatch([](float a) { return safe_acosf(a); });
    case NODE_MATH_ARCTANGENT:
      return dispatch([](float a) { return atanf(a); });
  }
  return false;
}

}  // namespace blender::nodes
