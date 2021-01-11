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

#include <iostream>

#include "BLI_math_vector.h"

namespace blender::math {

struct float3 {
  float x, y, z;

  float3() = default;

  float3(const float *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]}
  {
  }

  float3(const float (*ptr)[3]) : float3(static_cast<const float *>(ptr[0]))
  {
  }

  explicit float3(float value) : x(value), y(value), z(value)
  {
  }

  explicit float3(int value) : x(value), y(value), z(value)
  {
  }

  float3(float x, float y, float z) : x{x}, y{y}, z{z}
  {
  }

  operator const float *() const
  {
    return &x;
  }

  operator float *()
  {
    return &x;
  }

  uint64_t hash() const
  {
    uint64_t x1 = *reinterpret_cast<const uint32_t *>(&x);
    uint64_t x2 = *reinterpret_cast<const uint32_t *>(&y);
    uint64_t x3 = *reinterpret_cast<const uint32_t *>(&z);
    return (x1 * 435109) ^ (x2 * 380867) ^ (x3 * 1059217);
  }
};

inline float3 operator+(const float3 &a, const float3 &b)
{
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline float3 &operator+=(float3 &a, const float3 &b)
{
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
  return a;
}

inline float3 operator-(const float3 &a, const float3 &b)
{
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline float3 &operator-=(float3 &a, const float3 &b)
{
  a.x -= b.x;
  a.y -= b.y;
  a.z -= b.z;
  return a;
}

inline float3 &operator*=(float3 &a, float scalar)
{
  a.x *= scalar;
  a.y *= scalar;
  a.z *= scalar;
  return a;
}

inline float3 &operator*=(float3 &a, const float3 &other)
{
  a.x *= other.x;
  a.y *= other.y;
  a.z *= other.z;
  return a;
}

inline float3 operator*(const float3 &a, const float3 &b)
{
  return {a.x * b.x, a.y * b.y, a.z * b.z};
}

inline float3 operator/(const float3 &a, float b)
{
  BLI_assert(b != 0.0f);
  return {a.x / b, a.y / b, a.z / b};
}

inline std::ostream &operator<<(std::ostream &stream, const float3 &v)
{
  stream << "(" << v.x << ", " << v.y << ", " << v.z << ")";
  return stream;
}

inline float3 operator-(const float3 &a)
{
  return {-a.x, -a.y, -a.z};
}

inline bool operator==(const float3 &a, const float3 &b)
{
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool operator!=(const float3 &a, const float3 &b)
{
  return !(a == b);
}

inline float3 operator*(const float3 &a, float b)
{
  return {a.x * b, a.y * b, a.z * b};
}

inline float3 operator*(float a, const float3 &b)
{
  return b * a;
}

inline float normalize_and_get_length(float3 &a)
{
  return normalize_v3(a);
}

inline void normalize(float3 &a)
{
  normalize_v3(a);
}

inline float3 normalized(const float3 &a)
{
  float3 result;
  normalize_v3_v3(result, a);
  return result;
}

inline float length(const float3 &a)
{
  return len_v3(a);
}

inline float length_squared(const float3 &a)
{
  return len_squared_v3(a);
}

inline float3 reflected(const float3 &a, const float3 &normal)
{
  float3 result;
  reflect_v3_v3v3(result, a, normal);
  return result;
}

inline void reflect(float3 &a, const float3 &normal)
{
  a = reflected(a, normal);
}

inline float3 safe_divide(const float3 &a, const float3 &b)
{
  float3 result;
  result.x = (b.x == 0.0f) ? 0.0f : a.x / b.x;
  result.y = (b.y == 0.0f) ? 0.0f : a.y / b.y;
  result.z = (b.z == 0.0f) ? 0.0f : a.z / b.z;
  return result;
}

inline void negate(float3 &a)
{
  a.x = -a.x;
  a.y = -a.y;
  a.z = -a.z;
}

inline float dot(const float3 &a, const float3 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float3 cross_high_precision(const float3 &a, const float3 &b)
{
  float3 result;
  cross_v3_v3v3_hi_prec(result, a, b);
  return result;
}

inline float3 project(const float3 &a, const float3 &b)
{
  float3 result;
  project_v3_v3v3(result, a, b);
  return result;
}

inline float distance(const float3 &a, const float3 &b)
{
  return length(a - b);
}

inline float distance_squared(const float3 &a, const float3 &b)
{
  return dot(a, b);
}

inline float3 lerp(const float3 &a, const float3 &b, float t)
{
  return a * (1 - t) + b * t;
}

inline float3 abs(const float3 &a)
{
  return float3(fabsf(a.x), fabsf(a.y), fabsf(a.z));
}

}  // namespace blender::math

namespace blender {
using math::float3;
}
