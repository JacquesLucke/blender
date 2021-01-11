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

#include "BLI_double3.hh"

namespace blender::math {

struct double2 {
  double x, y;

  double2() = default;

  double2(const double *ptr) : x{ptr[0]}, y{ptr[1]}
  {
  }

  double2(double x, double y) : x(x), y(y)
  {
  }

  double2(const double3 &other) : x(other.x), y(other.y)
  {
  }

  operator double *()
  {
    return &x;
  }

  operator const double *() const
  {
    return &x;
  }

  struct isect_result {
    enum {
      LINE_LINE_COLINEAR = -1,
      LINE_LINE_NONE = 0,
      LINE_LINE_EXACT = 1,
      LINE_LINE_CROSS = 2,
    } kind;
    double lambda;
  };

  static isect_result isect_seg_seg(const double2 &v1,
                                    const double2 &v2,
                                    const double2 &v3,
                                    const double2 &v4);
};

inline double length(const double2 &a)
{
  return len_v2_db(a);
}

inline double2 operator+(const double2 &a, const double2 &b)
{
  return {a.x + b.x, a.y + b.y};
}

inline double2 operator-(const double2 &a, const double2 &b)
{
  return {a.x - b.x, a.y - b.y};
}

inline double2 operator*(const double2 &a, double b)
{
  return {a.x * b, a.y * b};
}

inline double2 operator/(const double2 &a, double b)
{
  BLI_assert(b != 0.0);
  return {a.x / b, a.y / b};
}

inline double2 operator*(double a, const double2 &b)
{
  return b * a;
}

inline bool operator==(const double2 &a, const double2 &b)
{
  return a.x == b.x && a.y == b.y;
}

inline bool operator!=(const double2 &a, const double2 &b)
{
  return a.x != b.x || a.y != b.y;
}

inline std::ostream &operator<<(std::ostream &stream, const double2 &v)
{
  stream << "(" << v.x << ", " << v.y << ")";
  return stream;
}

inline double dot(const double2 &a, const double2 &b)
{
  return a.x * b.x + a.y * b.y;
}

inline double2 lerp(const double2 &a, const double2 &b, double t)
{
  return a * (1 - t) + b * t;
}

inline double2 abs(const double2 &a)
{
  return double2(fabs(a.x), fabs(a.y));
}

inline double distance(const double2 &a, const double2 &b)
{
  return length(a - b);
}

inline double distance_squared(const double2 &a, const double2 &b)
{
  double2 diff = a - b;
  return dot(diff, diff);
}

}  // namespace blender::math

namespace blender {
using blender::math::double2;
}
