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

#ifndef __BLI_FLOAT4X4_H__
#define __BLI_FLOAT4X4_H__

#include "BLI_array_ref.h"
#include "BLI_float3.h"
#include "BLI_math_matrix.h"

namespace BLI {

struct float4x4 {
  float values[4][4];

  float4x4() = default;

  float4x4(float *matrix)
  {
    memcpy(values, matrix, sizeof(float) * 16);
  }

  float4x4(float matrix[4][4]) : float4x4((float *)matrix)
  {
  }

  operator float *()
  {
    return (float *)this;
  }

  float4x4 inverted() const
  {
    float result[4][4];
    invert_m4_m4(result, values);
    return result;
  }

  float4x4 inverted__LocRotScale() const
  {
    return this->inverted();
  }

  float3 transform_position(float3 position) const
  {
    mul_m4_v3(values, position);
    return position;
  }

  float3 transform_direction(float3 direction) const
  {
    mul_mat3_m4_v3(values, direction);
    return direction;
  }

  static void transform_positions(ArrayRef<float4x4> matrices,
                                  ArrayRef<float3> positions,
                                  MutableArrayRef<float3> r_results)
  {
    uint amount = matrices.size();
    BLI_assert(amount == positions.size());
    BLI_assert(amount == r_results.size());
    for (uint i = 0; i < amount; i++) {
      r_results[i] = matrices[i].transform_position(positions[i]);
    }
  }

  static void transform_directions(ArrayRef<float4x4> matrices,
                                   ArrayRef<float3> directions,
                                   MutableArrayRef<float3> r_results)
  {
    uint amount = matrices.size();
    BLI_assert(amount == directions.size());
    BLI_assert(amount == r_results.size());
    for (uint i = 0; i < amount; i++) {
      r_results[i] = matrices[i].transform_direction(directions[i]);
    }
  }

  static float4x4 interpolate(float4x4 a, float4x4 b, float t)
  {
    float result[4][4];
    interp_m4_m4m4(result, a.values, b.values, t);
    return result;
  }
};

}  // namespace BLI

#endif /* __BLI_FLOAT4X4_H__ */
