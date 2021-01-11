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

#  include "BLI_math_mpq.hh"
#  include "BLI_mpq3.hh"

namespace blender::math {

struct mpq2 {
  mpq_class x, y;

  mpq2() = default;

  mpq2(const mpq_class *ptr) : x{ptr[0]}, y{ptr[1]}
  {
  }

  mpq2(mpq_class x, mpq_class y) : x(x), y(y)
  {
  }

  mpq2(const mpq2 &other) : x(other.x), y(other.y)
  {
  }

  mpq2(mpq2 &&other) noexcept : x(std::move(other.x)), y(std::move(other.y))
  {
  }

  ~mpq2() = default;

  mpq2 &operator=(const mpq2 &other)
  {
    if (this != &other) {
      x = other.x;
      y = other.y;
    }
    return *this;
  }

  mpq2 &operator=(mpq2 &&other) noexcept
  {
    x = std::move(other.x);
    y = std::move(other.y);
    return *this;
  }

  mpq2(const mpq3 &other) : x(other.x), y(other.y)
  {
  }

  operator mpq_class *()
  {
    return &x;
  }

  operator const mpq_class *() const
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
    mpq_class lambda;
  };

  static isect_result isect_seg_seg(const mpq2 &v1,
                                    const mpq2 &v2,
                                    const mpq2 &v3,
                                    const mpq2 &v4);

  /** There is a sensible use for hashing on exact arithmetic types. */
  uint64_t hash() const;
};

inline mpq2 operator+(const mpq2 &a, const mpq2 &b)
{
  return {a.x + b.x, a.y + b.y};
}

inline mpq2 operator-(const mpq2 &a, const mpq2 &b)
{
  return {a.x - b.x, a.y - b.y};
}

inline mpq2 operator*(const mpq2 &a, mpq_class b)
{
  return {a.x * b, a.y * b};
}

inline mpq2 operator/(const mpq2 &a, mpq_class b)
{
  BLI_assert(b != 0);
  return {a.x / b, a.y / b};
}

inline mpq2 operator*(mpq_class a, const mpq2 &b)
{
  return b * a;
}

inline bool operator==(const mpq2 &a, const mpq2 &b)
{
  return a.x == b.x && a.y == b.y;
}

inline bool operator!=(const mpq2 &a, const mpq2 &b)
{
  return a.x != b.x || a.y != b.y;
}

inline std::ostream &operator<<(std::ostream &stream, const mpq2 &v)
{
  stream << "(" << v.x << ", " << v.y << ")";
  return stream;
}

inline mpq_class dot(const mpq2 &a, const mpq2 &b)
{
  return a.x * b.x + a.y * b.y;
}

/**
 * Cannot do this exactly in rational arithmetic!
 * Approximate by going in and out of doubles.
 */
inline mpq_class length(const mpq2 &a)
{
  mpq_class lsquared = dot(a, a);
  return mpq_class(sqrt(lsquared.get_d()));
}

inline mpq2 lerp(const mpq2 &a, const mpq2 &b, mpq_class t)
{
  return a * (1 - t) + b * t;
}

inline mpq2 abs(const mpq2 &a)
{
  mpq_class abs_x = (a.x >= 0) ? a.x : -a.x;
  mpq_class abs_y = (a.y >= 0) ? a.y : -a.y;
  return mpq2(abs_x, abs_y);
}

inline mpq_class distance(const mpq2 &a, const mpq2 &b)
{
  return length(a - b);
}

inline mpq_class distance_squared(const mpq2 &a, const mpq2 &b)
{
  return dot(a, b);
}

}  // namespace blender::math

namespace blender {
using math::mpq2;
}

#endif /* WITH_GMP */
