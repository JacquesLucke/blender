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
                                           Vector<uint> &r_filtered_pindices,
                                           Vector<float> &r_filtered_time_factors)
    : BlockStepDataAccess(step_data),
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
    : BlockStepDataAccess(step_data),
      m_pindices(pindices),
      m_current_times(current_times),
      m_event_storage(event_storage)
{
}

IntegratorInterface::IntegratorInterface(BlockStepData &step_data) : BlockStepDataAccess(step_data)
{
}

OffsetHandlerInterface::OffsetHandlerInterface(BlockStepData &step_data,
                                               ArrayRef<uint> pindices,
                                               ArrayRef<float> time_factors)
    : BlockStepDataAccess(step_data), m_pindices(pindices), m_time_factors(time_factors)
{
}

}  // namespace BParticles
