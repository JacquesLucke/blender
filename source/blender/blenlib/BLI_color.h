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

#ifndef __BLI_COLOR_H__
#define __BLI_COLOR_H__

#include "BLI_math_color.h"

namespace BLI {

struct rgba_f {
  float r, g, b, a;

  rgba_f() = default;

  rgba_f(float r, float g, float b, float a) : r(r), g(g), b(b), a(a)
  {
  }

  operator float *()
  {
    return &r;
  }

  operator std::array<float, 4>()
  {
    return {r, g, b, a};
  }

  friend std::ostream &operator<<(std::ostream &stream, rgba_f c)
  {
    stream << "(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
    return stream;
  }
};

struct rgba_b {
  uint8_t r, g, b, a;

  rgba_b() = default;

  rgba_b(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : r(r), g(g), b(b), a(a)
  {
  }

  rgba_b(rgba_f other)
  {
    rgba_float_to_uchar(*this, other);
  }

  operator rgba_f() const
  {
    rgba_f result;
    rgba_uchar_to_float(result, *this);
    return result;
  }

  operator uint8_t *()
  {
    return &r;
  }

  operator const uint8_t *() const
  {
    return &r;
  }
};

}  // namespace BLI

#endif /* __BLI_COLOR_H__ */
