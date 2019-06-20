#pragma once

namespace BParticles {

struct TimeSpan {
 private:
  float m_start, m_duration;

 public:
  TimeSpan(float start, float duration) : m_start(start), m_duration(duration)
  {
  }

  float start() const
  {
    return m_start;
  }

  float duration() const
  {
    return m_duration;
  }

  float end() const
  {
    return m_start + m_duration;
  }

  float interpolate(float t) const
  {
    return m_start + t * m_duration;
  }

  float get_factor(float time) const
  {
    BLI_assert(m_duration > 0.0f);
    return (time - m_start) / m_duration;
  }
};

}  // namespace BParticles
