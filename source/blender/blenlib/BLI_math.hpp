#pragma once

namespace BLI {

struct float3 {
  float x, y, z;

  float3() = default;

  float3(float *value) : x{value[0]}, y{value[1]}, z{value[2]}
  {
  }

  float3(float x, float y, float z) : x{x}, y{y}, z{z}
  {
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

  friend float3 operator/(float3 a, float b)
  {
    BLI_assert(b != 0);
    return {a.x / b, a.y / b, a.z / b};
  }
};

}  // namespace BLI
