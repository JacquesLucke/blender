#pragma once

#include "particles_container.hpp"

namespace BParticles {

class ForceInterface {
 private:
  ParticlesBlock &m_block;
  ArrayAllocator &m_array_allocator;
  ArrayRef<float> m_remaining_durations;
  float m_step_end_time;
  ArrayRef<float3> m_destination;

 public:
  ForceInterface(ParticlesBlock &block,
                 ArrayAllocator &array_allocator,
                 ArrayRef<float> remaining_durations,
                 float step_end_time,
                 ArrayRef<float3> destination)
      : m_block(block),
        m_array_allocator(array_allocator),
        m_remaining_durations(remaining_durations),
        m_step_end_time(step_end_time),
        m_destination(destination)
  {
  }

  ParticlesBlock &block()
  {
    return m_block;
  }

  ArrayAllocator &array_allocator()
  {
    return m_array_allocator;
  }

  ArrayRef<float3> combined_destination()
  {
    return m_destination;
  }

  ArrayRef<float> remaining_durations()
  {
    return m_remaining_durations;
  }

  float step_end_time()
  {
    return m_step_end_time;
  }
};

}  // namespace BParticles
