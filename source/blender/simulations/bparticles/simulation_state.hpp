#pragma once

#include "BLI_float_interval.h"

#include "particles_state.hpp"
#include "world_state.hpp"

namespace BParticles {

using BLI::FloatInterval;

class SimulationTimeState {
 private:
  bool m_is_updating = false;
  float m_simulation_time = 0.0f;
  float m_update_start_time = 0.0f;
  float m_update_duration = 0.0f;
  uint m_current_update_index = 0;

 public:
  bool is_updating() const
  {
    return m_is_updating;
  }

  FloatInterval current_update_time() const
  {
    BLI_assert(m_is_updating);
    return FloatInterval(m_update_start_time, m_update_duration);
  }

  uint current_update_index() const
  {
    BLI_assert(m_is_updating);
    return m_current_update_index;
  }

  void start_update(float time_step)
  {
    BLI_assert(time_step >= 0);
    BLI_assert(!m_is_updating);
    m_is_updating = true;
    m_update_start_time = m_simulation_time;
    m_update_duration = time_step;
    m_current_update_index++;
  }

  void end_update()
  {
    BLI_assert(m_is_updating);
    m_is_updating = false;
    m_simulation_time = m_update_start_time + m_update_duration;
  }
};

class SimulationState {
 private:
  ParticlesState m_particles;
  WorldState m_world;
  SimulationTimeState m_time_state;

 public:
  ParticlesState &particles()
  {
    return m_particles;
  }

  WorldState &world()
  {
    return m_world;
  }

  SimulationTimeState &time()
  {
    return m_time_state;
  }
};

}  // namespace BParticles
