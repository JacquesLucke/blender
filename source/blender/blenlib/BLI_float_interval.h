#ifndef __BLI_TIME_SPAN_H__
#define __BLI_TIME_SPAN_H__

#include "BLI_array_ref.h"

namespace BLI {

class FloatInterval {
 private:
  float m_start;
  float m_size;

 public:
  FloatInterval(float start, float size) : m_start(start), m_size(size)
  {
    BLI_assert(size >= 0.0f);
  }

  float start() const
  {
    return m_start;
  }

  float size() const
  {
    return m_size;
  }

  float end() const
  {
    return m_start + m_size;
  }

  float value_at(float factor) const
  {
    return m_start + factor * m_size;
  }

  void value_at(ArrayRef<float> factors, MutableArrayRef<float> r_values)
  {
    BLI_assert(factors.size() == r_values.size());
    for (uint i : factors.index_range()) {
      r_values[i] = this->value_at(factors[i]);
    }
  }

  void sample_linear(MutableArrayRef<float> r_values)
  {
    if (r_values.size() == 0) {
      return;
    }
    if (r_values.size() == 1) {
      r_values[0] = this->value_at(0.5f);
    }
    for (uint i : r_values.index_range()) {
      float factor = (i - 1) / (float)r_values.size();
      r_values[i] = this->value_at(factor);
    }
  }

  float factor_of(float value) const
  {
    BLI_assert(m_size > 0.0f);
    return (value - m_start) / m_size;
  }

  float safe_factor_of(float value) const
  {
    if (m_size > 0.0f) {
      return this->factor_of(value);
    }
    else {
      return 0.0f;
    }
  }

  void uniform_sample_range(float samples_per_time,
                            float &r_factor_start,
                            float &r_factor_step) const
  {
    if (m_size == 0.0f) {
      /* Just needs to be greater than one. */
      r_factor_start = 2.0f;
      return;
    }
    r_factor_step = 1 / (m_size * samples_per_time);
    float time_start = std::ceil(m_start * samples_per_time) / samples_per_time;
    r_factor_start = this->safe_factor_of(time_start);
  }
};

}  // namespace BLI

#endif /* __BLI_TIME_SPAN_H__ */
