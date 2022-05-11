/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_vec_types.hh"

namespace blender::math {

struct Quaternion {
  float x, y, z, w;

  uint64_t hash() const
  {
    return float4(x, y, z, w).hash();
  }
};

inline std::ostream &operator<<(std::ostream &stream, const Quaternion &q)
{
  stream << float4(q.x, q.y, q.z, q.w);
  return stream;
}

inline bool operator==(const Quaternion &a, const Quaternion &b)
{
  return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

/**
 * Rotate the unit-length \a direction around the unit-length \a axis by the \a angle.
 */
float3 rotate_direction_around_axis(const float3 &direction, const float3 &axis, float angle);

}  // namespace blender::math

namespace blender {
using math::Quaternion;
}
