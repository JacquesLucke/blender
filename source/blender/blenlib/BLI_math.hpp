#pragma once

#include <array>

#include "BLI_math_vector.h"
#include "BLI_math_matrix.h"

namespace BLI {

struct float2;
struct float3;
struct rgba_f;
struct rgba_b;

struct float2 {
  float x, y;

  float2() = default;

  float2(const float *ptr) : x{ptr[0]}, y{ptr[1]}
  {
  }

  float2(float x, float y) : x(x), y(y)
  {
  }

  float2(float3 other);

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

struct float3 {
  float x, y, z;

  float3() = default;

  float3(const float *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]}
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
    return (const float *)this;
  }

  operator float *()
  {
    return (float *)this;
  }

  operator std::array<float, 3>()
  {
    return {x, y, z};
  }

  float normalize_and_get_length()
  {
    return normalize_v3(*this);
  }

  float3 normalized() const
  {
    float3 result;
    normalize_v3_v3(result, *this);
    return result;
  }

  float length() const
  {
    return len_v3(*this);
  }

  void reflect(float3 normal)
  {
    *this = this->reflected(normal);
  }

  float3 reflected(float3 normal) const
  {
    float3 result;
    reflect_v3_v3v3(result, *this, normal);
    return result;
  }

  void invert()
  {
    x = -x;
    y = -y;
    z = -z;
  }

  bool is_zero() const
  {
    return is_zero_v3(*this);
  }

  friend float3 operator+(float3 a, float3 b)
  {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
  }

  void operator+=(float3 b)
  {
    this->x += b.x;
    this->y += b.y;
    this->z += b.z;
  }

  friend float3 operator-(float3 a, float3 b)
  {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
  }

  friend float3 operator-(float3 a)
  {
    return {-a.x, -a.y, -a.z};
  }

  void operator-=(float3 b)
  {
    this->x -= b.x;
    this->y -= b.y;
    this->z -= b.z;
  }

  void operator*=(float scalar)
  {
    this->x *= scalar;
    this->y *= scalar;
    this->z *= scalar;
  }

  friend float3 operator*(float3 a, float3 b)
  {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
  }

  friend float3 operator*(float3 a, float b)
  {
    return {a.x * b, a.y * b, a.z * b};
  }

  friend float3 operator*(float a, float3 b)
  {
    return b * a;
  }

  friend float3 operator/(float3 a, float b)
  {
    BLI_assert(b != 0);
    return {a.x / b, a.y / b, a.z / b};
  }

  friend std::ostream &operator<<(std::ostream &stream, float3 v)
  {
    stream << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return stream;
  }

  static float dot(float3 a, float3 b)
  {
    return a.x * b.x + a.y * b.y + a.z * b.z;
  }

  static float distance(float3 a, float3 b)
  {
    return (a - b).length();
  }

  static float3 interpolate(float3 a, float3 b, float t)
  {
    return a * (1 - t) + b * t;
  }
};

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

  float3 transform_position(float3 position)
  {
    mul_m4_v3(values, position);
    return position;
  }

  float3 transform_direction(float3 direction)
  {
    mul_mat3_m4_v3(values, direction);
    return direction;
  }

  static float4x4 interpolate(float4x4 a, float4x4 b, float t)
  {
    float result[4][4];
    interp_m4_m4m4(result, a.values, b.values, t);
    return result;
  }
};

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

  operator rgba_f()
  {
    rgba_f result;
    rgba_uchar_to_float(result, *this);
    return result;
  }

  operator uint8_t *()
  {
    return &r;
  }
};

/* Conversions
 *****************************************/

inline float2::float2(float3 other) : x(other.x), y(other.y)
{
}

}  // namespace BLI
