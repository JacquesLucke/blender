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

#include "NOD_math_functions.hh"

namespace blender::nodes {

const FloatMathOperationInfo *get_float_math_operation_info(const int operation)
{
  switch (operation) {
    case NODE_MATH_ADD: {
      static const FloatMathOperationInfo info{"Add", "math_add"};
      return &info;
    }
    case NODE_MATH_SUBTRACT: {
      static const FloatMathOperationInfo info{"Subtract", "math_subtract"};
      return &info;
    }
    case NODE_MATH_MULTIPLY: {
      static const FloatMathOperationInfo info{"Multiply", "math_multiply"};
      return &info;
    }
    case NODE_MATH_DIVIDE: {
      static const FloatMathOperationInfo info{"Divide", "math_divide"};
      return &info;
    }
    case NODE_MATH_MULTIPLY_ADD: {
      static const FloatMathOperationInfo info{"Multiply Add", "math_multiply_add"};
      return &info;
    }
    case NODE_MATH_POWER: {
      static const FloatMathOperationInfo info{"Power", "math_power"};
      return &info;
    }
  }
  return nullptr;
}

}  // namespace blender::nodes
