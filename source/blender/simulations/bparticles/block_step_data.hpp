#pragma once

#include "attributes.hpp"
#include "time_span.hpp"

namespace BParticles {

struct BlockStepData {
  AttributesRef attributes;
  AttributesRef attribute_offsets;
  MutableArrayRef<float> remaining_durations;
  float step_end_time;

  uint array_size()
  {
    return this->remaining_durations.size();
  }
};

class BlockStepDataAccess {
 protected:
  BlockStepData &m_step_data;

 public:
  BlockStepDataAccess(BlockStepData &step_data) : m_step_data(step_data)
  {
  }

  uint array_size() const
  {
    return m_step_data.array_size();
  }

  BlockStepData &step_data()
  {
    return m_step_data;
  }

  AttributesRef attributes()
  {
    return m_step_data.attributes;
  }

  AttributesRef attribute_offsets()
  {
    return m_step_data.attribute_offsets;
  }

  MutableArrayRef<float> remaining_durations()
  {
    return m_step_data.remaining_durations;
  }

  float step_end_time()
  {
    return m_step_data.step_end_time;
  }

  TimeSpan time_span(uint pindex)
  {
    float duration = m_step_data.remaining_durations[pindex];
    return TimeSpan(m_step_data.step_end_time - duration, duration);
  }
};

}  // namespace BParticles
