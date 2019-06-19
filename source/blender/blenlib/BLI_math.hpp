#pragma once

#include "BLI_math_matrix.h"

namespace BLI {

struct float3 {
  float x, y, z;

  float3() = default;

  float3(const float *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]}
  {
  }

  float3(float x, float y, float z) : x{x}, y{y}, z{z}
  {
  }

  operator float *()
  {
    return (float *)this;
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
};

struct float4x4 {
  float v[4][4];

  float4x4(float *matrix)
  {
    memcpy(v, matrix, sizeof(float) * 16);
  }

  float4x4(float matrix[4][4]) : float4x4((float *)matrix)
  {
  }

  operator float *()
  {
    return (float *)this;
  }

  float3 transform_position(float3 position)
  {
    mul_m4_v3((float(*)[4])this, position);
    return position;
  }
};

}  // namespace BLI
