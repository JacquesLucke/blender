#include "events.hpp"

namespace BParticles {

class AgeReachedEvent : public Event {
 private:
  float m_age;

 public:
  AgeReachedEvent(float age) : m_age(age)
  {
  }

  void filter(AttributeArrays attributes,
              ArrayRef<uint> particle_indices,
              IdealOffsets &UNUSED(ideal_offsets),
              ArrayRef<float> durations,
              float end_time,
              SmallVector<uint> &r_filtered_indices,
              SmallVector<float> &r_time_factors) override
  {
    auto birth_times = attributes.get_float("Birth Time");

    for (uint i = 0; i < particle_indices.size(); i++) {
      uint pindex = particle_indices[i];
      float duration = durations[i];
      float birth_time = birth_times[pindex];
      float age = end_time - birth_time;
      if (age >= m_age && age - duration < m_age) {
        r_filtered_indices.append(i);
        float time_factor =
            TimeSpan(end_time - duration, duration).get_factor(birth_time + m_age) + 0.00001f;
        r_time_factors.append(time_factor);
      }
    }
  }
};

std::unique_ptr<Event> EVENT_age_reached(float age)
{
  Event *event = new AgeReachedEvent(age);
  return std::unique_ptr<Event>(event);
}

}  // namespace BParticles
