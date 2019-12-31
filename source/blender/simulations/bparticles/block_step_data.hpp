#pragma once

#include "FN_attributes_ref.h"

#include "BLI_float_interval.h"

#include "simulation_state.hpp"

namespace BParticles {

using BLI::FloatInterval;
using FN::AttributesRef;
using FN::MutableAttributesRef;

struct BlockStepData {
  SimulationState &simulation_state;
  MutableAttributesRef attributes;
  MutableAttributesRef attribute_offsets;
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

  SimulationState &simulation_state()
  {
    return m_step_data.simulation_state;
  }

  uint array_size() const
  {
    return m_step_data.array_size();
  }

  BlockStepData &step_data()
  {
    return m_step_data;
  }

  MutableAttributesRef attributes()
  {
    return m_step_data.attributes;
  }

  MutableAttributesRef attribute_offsets()
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

  FloatInterval time_span(uint pindex)
  {
    float duration = m_step_data.remaining_durations[pindex];
    return FloatInterval(m_step_data.step_end_time - duration, duration);
  }
};

}  // namespace BParticles
