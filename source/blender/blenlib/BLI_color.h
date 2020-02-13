#ifndef __BLI_COLOR_H__
#define __BLI_COLOR_H__

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
