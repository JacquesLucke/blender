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

/** \file
 * \ingroup bli
 */

#include <iostream>

#include "BLI_math_vector.h"
#include "BLI_span.hh"

namespace blender::math {

struct double3 {
  double x, y, z;

  double3() = default;

  double3(const double *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]}
  {
  }

  double3(const double (*ptr)[3]) : double3((const double *)ptr)
  {
  }

  explicit double3(double value) : x(value), y(value), z(value)
  {
  }

  explicit double3(int value) : x(value), y(value), z(value)
  {
  }

  double3(double x, double y, double z) : x{x}, y{y}, z{z}
  {
  }

  operator const double *() const
  {
    return &x;
  }

  operator double *()
  {
    return &x;
  }

  static double3 cross_poly(Span<double3> poly);
};

inline double normalize_and_get_length(double3 &a)
{
  return normalize_v3_db(a);
}

inline double3 normalized(const double3 &a)
{
  double3 result;
  normalize_v3_v3_db(result, a);
  return result;
}

inline double length(const double3 &a)
{
  return len_v3_db(a);
}

inline double length_squared(const double3 &a)
{
  return len_squared_v3_db(a);
}

inline double3 reflected(const double3 &a, const double3 &normal)
{
  double3 result;
  reflect_v3_v3v3_db(result, a, normal);
  return result;
}

inline void reflect(double3 &a, const double3 &normal)
{
  a = reflected(a, normal);
}

inline double3 safe_divide(const double3 &a, const double3 &b)
{
  double3 result;
  result.x = (b.x == 0.0) ? 0.0 : a.x / b.x;
  result.y = (b.y == 0.0) ? 0.0 : a.y / b.y;
  result.z = (b.z == 0.0) ? 0.0 : a.z / b.z;
  return result;
}

inline void negate(double3 &a)
{
  a.x = -a.x;
  a.y = -a.y;
  a.z = -a.z;
}

inline double3 operator+(const double3 &a, const double3 &b)
{
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline void operator+=(double3 &a, const double3 &b)
{
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
}

inline double3 operator-(const double3 &a, const double3 &b)
{
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline double3 operator-(const double3 &a)
{
  return {-a.x, -a.y, -a.z};
}

inline void operator-=(double3 &a, const double3 &b)
{
  a.x -= b.x;
  a.y -= b.y;
  a.z -= b.z;
}

inline void operator*=(double3 &a, const double &scalar)
{
  a.x *= scalar;
  a.y *= scalar;
  a.z *= scalar;
}

inline void operator*=(double3 &a, const double3 &other)
{
  a.x *= other.x;
  a.y *= other.y;
  a.z *= other.z;
}

inline double3 operator*(const double3 &a, const double3 &b)
{
  return {a.x * b.x, a.y * b.y, a.z * b.z};
}

inline double3 operator*(const double3 &a, const double &b)
{
  return {a.x * b, a.y * b, a.z * b};
}

inline double3 operator*(const double &a, const double3 &b)
{
  return b * a;
}

inline double3 operator/(const double3 &a, const double &b)
{
  BLI_assert(b != 0.0);
  return {a.x / b, a.y / b, a.z / b};
}

inline bool operator==(const double3 &a, const double3 &b)
{
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool operator!=(const double3 &a, const double3 &b)
{
  return a.x != b.x || a.y != b.y || a.z != b.z;
}

inline std::ostream &operator<<(std::ostream &stream, const double3 &v)
{
  stream << "(" << v.x << ", " << v.y << ", " << v.z << ")";
  return stream;
}

inline double dot(const double3 &a, const double3 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline double3 cross_high_precision(const double3 &a, const double3 &b)
{
  double3 result;
  cross_v3_v3v3_db(result, a, b);
  return result;
}

inline double3 project(const double3 &a, const double3 &b)
{
  double3 result;
  project_v3_v3v3_db(result, a, b);
  return result;
}

inline double distance(const double3 &a, const double3 &b)
{
  return length(a - b);
}

inline double distance_squared(const double3 &a, const double3 &b)
{
  return dot(a, b);
}

inline double3 lerp(const double3 &a, const double3 &b, double t)
{
  return a * (1 - t) + b * t;
}

inline double3 abs(const double3 &a)
{
  return double3(fabs(a.x), fabs(a.y), fabs(a.z));
}

inline int dominant_axis(const double3 &a)
{
  double x = (a.x >= 0) ? a.x : -a.x;
  double y = (a.y >= 0) ? a.y : -a.y;
  double z = (a.z >= 0) ? a.z : -a.z;
  return ((x > y) ? ((x > z) ? 0 : 2) : ((y > z) ? 1 : 2));
}

}  // namespace blender::math

namespace blender {
using math::double3;
}
