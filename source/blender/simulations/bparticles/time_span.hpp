#pragma once

namespace BParticles {

/**
 * Contains a time range defined by a start time and non-zero duration. The times are measured in
 * seconds.
 */
struct TimeSpan {
 private:
  float m_start, m_duration;

 public:
  TimeSpan(float start, float duration) : m_start(start), m_duration(duration)
  {
  }

  /**
   * Get the beginning of the time span.
   */
  float start() const
  {
    return m_start;
  }

  /**
   * Get the duration of the time span.
   */
  float duration() const
  {
    return m_duration;
  }

  /**
   * Get the end of the time span.
   */
  float end() const
  {
    return m_start + m_duration;
  }

  /**
   * Compute a point in time within this time step. Usually 0 <= t <= 1.
   */
  float interpolate(float t) const
  {
    return m_start + t * m_duration;
  }

  void interpolate(ArrayRef<float> times, MutableArrayRef<float> r_results)
  {
    BLI_assert(times.size() == r_results.size());
    for (uint i = 0; i < times.size(); i++) {
      r_results[i] = this->interpolate(times[i]);
    }
  }

  /**
   * The reverse of interpolate.
   * Asserts when the duration is 0.
   */
  float get_factor(float time) const
  {
    BLI_assert(m_duration > 0.0f);
    return (time - m_start) / m_duration;
  }

  /**
   * Same as get_factor, but returns zero when the duration is zero.
   */
  float get_factor_safe(float time) const
  {
    if (m_duration > 0) {
      return this->get_factor(time);
    }
    else {
      return 0.0f;
    }
  }
};

}  // namespace BParticles
