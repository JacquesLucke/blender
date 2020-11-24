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

  switch (operation) {
    case NODE_MATH_ADD: {
      op([](float a, float b) { return a + b; }, *info);
      return true;
    }
    case NODE_MATH_SUBTRACT: {
      op([](float a, float b) { return a - b; }, *info);
      return true;
    }
    case NODE_MATH_MULTIPLY: {
      op([](float a, float b) { return a * b; }, *info);
      return true;
    }
    case NODE_MATH_DIVIDE: {
      op(safe_divide, *info);
      return true;
    }
    case NODE_MATH_POWER: {
      op(safe_powf, *info);
      return true;
    }
    case NODE_MATH_LOGARITHM: {
      op(safe_logf, *info);
      return true;
    }
    case NODE_MATH_MINIMUM: {
      op([](float a, float b) { return std::min(a, b); }, *info);
      return true;
    }
    case NODE_MATH_MAXIMUM: {
      op([](float a, float b) { return std::max(a, b); }, *info);
      return true;
    }
    case NODE_MATH_LESS_THAN: {
      op([](float a, float b) { return (float)(a < b); }, *info);
      return true;
    }
    case NODE_MATH_GREATER_THAN: {
      op([](float a, float b) { return (float)(a > b); }, *info);
      return true;
    }
    case NODE_MATH_MODULO: {
      op(safe_modf, *info);
      return true;
    }
    case NODE_MATH_TRUNC: {
      op([](float a, float b) { return a >= 0.0f ? floorf(a) : ceilf(a); }, *info);
      return true;
    }
    case NODE_MATH_SNAP: {
      op([](float a, float b) { return floorf(safe_divide(a, b)) * b; }, *info);
      return true;
    }
    case NODE_MATH_ARCTAN2: {
      op([](float a, float b) { return atan2f(a, b); }, *info);
      return true;
    }
  }
  return false;
}

}  // namespace blender::nodes
