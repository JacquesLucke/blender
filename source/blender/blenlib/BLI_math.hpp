#pragma once

namespace BLI {

struct Vec3 {
  float x, y, z;

  friend Vec3 operator+(Vec3 a, Vec3 b)
  {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
  }

  void operator+=(Vec3 b)
  {
    this->x += b.x;
    this->y += b.y;
    this->z += b.z;
  }

  friend Vec3 operator*(Vec3 a, Vec3 b)
  {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
  }

  friend Vec3 operator*(Vec3 a, float b)
  {
    return {a.x * b, a.y * b, a.z * b};
  }
};

}  // namespace BLI
