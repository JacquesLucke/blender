#include "step_description_interfaces.hpp"

namespace BParticles {

EmitterInterface::EmitterInterface(ParticleAllocator &particle_allocator,
                                   ArrayAllocator &array_allocator,
                                   TimeSpan time_span)
    : m_particle_allocator(particle_allocator),
      m_array_allocator(array_allocator),
      m_time_span(time_span)
{
}

EventFilterInterface::EventFilterInterface(BlockStepData &step_data,
                                           ArrayRef<uint> pindices,
                                           ArrayRef<float> known_min_time_factors,
                                           EventStorage &r_event_storage,
                                           SmallVector<uint> &r_filtered_pindices,
                                           SmallVector<float> &r_filtered_time_factors)
    : m_step_data(step_data),
      m_pindices(pindices),
      m_known_min_time_factors(known_min_time_factors),
      m_event_storage(r_event_storage),
      m_filtered_pindices(r_filtered_pindices),
      m_filtered_time_factors(r_filtered_time_factors)
{
}

EventExecuteInterface::EventExecuteInterface(BlockStepData &step_data,
                                             ArrayRef<uint> pindices,
                                             ArrayRef<float> current_times,
                                             EventStorage &event_storage)
    : m_step_data(step_data),
      m_pindices(pindices),
      m_current_times(current_times),
      m_event_storage(event_storage)
{
}

IntegratorInterface::IntegratorInterface(ParticlesBlock &block,
                                         ArrayRef<float> durations,
                                         ArrayAllocator &array_allocator,
                                         AttributeArrays r_offsets)
    : m_block(block),
      m_durations(durations),
      m_array_allocator(array_allocator),
      m_offsets(r_offsets)
{
}

OffsetHandlerInterface::OffsetHandlerInterface(BlockStepData &step_data,
                                               ArrayRef<uint> pindices,
                                               ArrayRef<float> time_factors)
    : m_step_data(step_data), m_pindices(pindices), m_time_factors(time_factors)
{
}

}  // namespace BParticles
