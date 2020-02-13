#ifndef __BLI_FLOAT2_H__
#define __BLI_FLOAT2_H__

#include "BLI_float3.h"

namespace BLI {

struct float2 {
  float x, y;

  float2() = default;

  float2(const float *ptr) : x{ptr[0]}, y{ptr[1]}
  {
  }

  float2(float x, float y) : x(x), y(y)
  {
  }

  float2(float3 other) : x(other.x), y(other.y)
  {
  }

  operator float *()
  {
    return &x;
  }

  float2 clamped(float min, float max)
  {
    return {std::min(std::max(x, min), max), std::min(std::max(y, min), max)};
  }

  float2 clamped_01()
  {
    return this->clamped(0, 1);
  }

  friend float2 operator+(float2 a, float2 b)
  {
    return {a.x + b.x, a.y + b.y};
  }

  friend float2 operator-(float2 a, float2 b)
  {
    return {a.x - b.x, a.y - b.y};
  }

  friend float2 operator*(float2 a, float b)
  {
    return {a.x * b, a.y * b};
  }

  friend float2 operator/(float2 a, float b)
  {
    return {a.x / b, a.y / b};
  }

  friend float2 operator*(float a, float2 b)
  {
    return b * a;
  }

  friend std::ostream &operator<<(std::ostream &stream, float2 v)
  {
    stream << "(" << v.x << ", " << v.y << ")";
    return stream;
  }
};

}  // namespace BLI

#endif /* __BLI_FLOAT2_H__ */
