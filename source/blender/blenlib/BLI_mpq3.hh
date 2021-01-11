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

#ifdef WITH_GMP

#  include <iostream>

#  include "BLI_math.h"
#  include "BLI_math_mpq.hh"
#  include "BLI_span.hh"

namespace blender::math {

struct mpq3 {
  mpq_class x, y, z;

  mpq3() = default;

  mpq3(const mpq_class *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]}
  {
  }

  mpq3(const mpq_class (*ptr)[3]) : mpq3((const mpq_class *)ptr)
  {
  }

  explicit mpq3(mpq_class value) : x(value), y(value), z(value)
  {
  }

  explicit mpq3(int value) : x(value), y(value), z(value)
  {
  }

  mpq3(mpq_class x, mpq_class y, mpq_class z) : x{x}, y{y}, z{z}
  {
  }

  operator const mpq_class *() const
  {
    return &x;
  }

  operator mpq_class *()
  {
    return &x;
  }

  static mpq3 cross_poly(Span<mpq3> poly);

  /** There is a sensible use for hashing on exact arithmetic types. */
  uint64_t hash() const;
};

/* Cannot do this exactly in rational arithmetic!
 * Approximate by going in and out of doubles.
 */
inline mpq_class normalize_and_get_length(mpq3 &a)
{
  double dv[3] = {a.x.get_d(), a.y.get_d(), a.z.get_d()};
  double len = normalize_v3_db(dv);
  a.x = mpq_class(dv[0]);
  a.y = mpq_class(dv[1]);
  a.z = mpq_class(dv[2]);
  return len;
}

inline mpq3 normalized(const mpq3 &a)
{
  double dv[3] = {a.x.get_d(), a.y.get_d(), a.z.get_d()};
  double dr[3];
  normalize_v3_v3_db(dr, dv);
  return mpq3(mpq_class(dr[0]), mpq_class(dr[1]), mpq_class(dr[2]));
}

inline mpq_class length_squared(const mpq3 &a)
{
  return a.x * a.x + a.y * a.y + a.z * a.z;
}

/* Cannot do this exactly in rational arithmetic!
 * Approximate by going in and out of double.
 */
inline mpq_class length(const mpq3 &a)
{
  mpq_class lsquared = length_squared(a);
  double dsquared = lsquared.get_d();
  double d = sqrt(dsquared);
  return mpq_class(d);
}

inline mpq_class dot(const mpq3 &a, const mpq3 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline mpq3 reflected(const mpq3 &a, const mpq3 &normal)
{
  mpq3 result;
  const mpq_class dot2 = 2 * dot(a, normal);
  result.x = a.x - (dot2 * normal.x);
  result.y = a.y - (dot2 * normal.y);
  result.z = a.z - (dot2 * normal.z);
  return result;
}

inline void reflect(mpq3 &a, const mpq3 &normal)
{
  a = reflected(a, normal);
}

inline mpq3 safe_divide(const mpq3 &a, const mpq3 &b)
{
  mpq3 result;
  result.x = (b.x == 0) ? mpq_class(0) : a.x / b.x;
  result.y = (b.y == 0) ? mpq_class(0) : a.y / b.y;
  result.z = (b.z == 0) ? mpq_class(0) : a.z / b.z;
  return result;
}

inline void negate(mpq3 &a)
{
  a.x = -a.x;
  a.y = -a.y;
  a.z = -a.z;
}

inline mpq3 operator+(const mpq3 &a, const mpq3 &b)
{
  return mpq3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline void operator+=(mpq3 &a, const mpq3 &b)
{
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
}

inline mpq3 operator-(const mpq3 &a, const mpq3 &b)
{
  return mpq3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline mpq3 operator-(const mpq3 &a)
{
  return mpq3(-a.x, -a.y, -a.z);
}

inline void operator-=(mpq3 &a, const mpq3 &b)
{
  a.x -= b.x;
  a.y -= b.y;
  a.z -= b.z;
}

inline void operator*=(mpq3 &a, mpq_class scalar)
{
  a.x *= scalar;
  a.y *= scalar;
  a.z *= scalar;
}

inline void operator*=(mpq3 &a, const mpq3 &other)
{
  a.x *= other.x;
  a.y *= other.y;
  a.z *= other.z;
}

inline mpq3 operator*(const mpq3 &a, const mpq3 &b)
{
  return {a.x * b.x, a.y * b.y, a.z * b.z};
}

inline mpq3 operator*(const mpq3 &a, const mpq_class &b)
{
  return mpq3(a.x * b, a.y * b, a.z * b);
}

inline mpq3 operator*(const mpq_class &a, const mpq3 &b)
{
  return mpq3(a * b.x, a * b.y, a * b.z);
}

inline mpq3 operator/(const mpq3 &a, const mpq_class &b)
{
  BLI_assert(b != 0);
  return mpq3(a.x / b, a.y / b, a.z / b);
}

inline bool operator==(const mpq3 &a, const mpq3 &b)
{
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool operator!=(const mpq3 &a, const mpq3 &b)
{
  return a.x != b.x || a.y != b.y || a.z != b.z;
}

inline std::ostream &operator<<(std::ostream &stream, const mpq3 &v)
{
  stream << "(" << v.x << ", " << v.y << ", " << v.z << ")";
  return stream;
}

inline mpq3 cross(const mpq3 &a, const mpq3 &b)
{
  return mpq3(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
}

inline mpq3 cross_high_precision(const mpq3 &a, const mpq3 &b)
{
  return cross(a, b);
}

inline mpq3 project(const mpq3 &a, const mpq3 &b)
{
  const mpq_class mul = dot(a, b) / dot(b, b);
  return mpq3(mul * b[0], mul * b[1], mul * b[2]);
}

inline mpq_class distance(const mpq3 &a, const mpq3 &b)
{
  return length(a - b);
}

inline mpq_class distance_squared(const mpq3 &a, const mpq3 &b)
{
  mpq3 diff(a.x - b.x, a.y - b.y, a.z - b.z);
  return dot(diff, diff);
}

inline mpq3 lerp(const mpq3 &a, const mpq3 &b, mpq_class t)
{
  mpq_class s = 1 - t;
  return mpq3(a.x * s + b.x * t, a.y * s + b.y * t, a.z * s + b.z * t);
}

inline mpq3 abs(const mpq3 &a)
{
  mpq_class abs_x = (a.x >= 0) ? a.x : -a.x;
  mpq_class abs_y = (a.y >= 0) ? a.y : -a.y;
  mpq_class abs_z = (a.z >= 0) ? a.z : -a.z;
  return mpq3(abs_x, abs_y, abs_z);
}

inline int dominant_axis(const mpq3 &a)
{
  mpq_class x = (a.x >= 0) ? a.x : -a.x;
  mpq_class y = (a.y >= 0) ? a.y : -a.y;
  mpq_class z = (a.z >= 0) ? a.z : -a.z;
  return ((x > y) ? ((x > z) ? 0 : 2) : ((y > z) ? 1 : 2));
}

}  // namespace blender::math

namespace blender {
using math::mpq3;

uint64_t hash_mpq_class(const mpq_class &value);
}  // namespace blender

#endif /* WITH_GMP */
