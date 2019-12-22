#include "particle_action.hpp"

BLI_CREATE_CLASS_ID(BParticles::ParticleCurrentTimesContext)
BLI_CREATE_CLASS_ID(BParticles::ParticleIntegratedOffsets)
BLI_CREATE_CLASS_ID(BParticles::ParticleRemainingTimeInStep)

namespace BParticles {

using BLI::LargeScopedArray;

ParticleAction::~ParticleAction()
{
}

void ParticleAction::execute_from_emitter(AttributesRefGroup &new_particles,
                                          EmitterInterface &emitter_interface)
{
  for (AttributesRef attributes : new_particles) {
    ParticleCurrentTimesContext current_times_context;
    current_times_context.current_times = attributes.get<float>("Birth Time");

    ParticleActionContext context(emitter_interface.particle_allocator(),
                                  IndexMask(attributes.size()),
                                  attributes,
                                  {BLI::get_class_id<ParticleCurrentTimesContext>()},
                                  {(void *)&current_times_context});
    this->execute(context);
  }
}

void ParticleAction::execute_for_new_particles(AttributesRefGroup &new_particles,
                                               ParticleActionContext &parent_context)
{

  for (AttributesRef attributes : new_particles) {
    ParticleCurrentTimesContext current_times_context;
    current_times_context.current_times = attributes.get<float>("Birth Time");

    ParticleActionContext context(parent_context.particle_allocator(),
                                  IndexMask(attributes.size()),
                                  attributes,
                                  {BLI::get_class_id<ParticleCurrentTimesContext>()},
                                  {(void *)&current_times_context});
    this->execute(context);
  }
}

void ParticleAction::execute_for_new_particles(AttributesRefGroup &new_particles,
                                               OffsetHandlerInterface &offset_handler_interface)
{
  for (AttributesRef attributes : new_particles) {
    ParticleCurrentTimesContext current_times_context;
    current_times_context.current_times = attributes.get<float>("Birth Time");

    ParticleActionContext context(offset_handler_interface.particle_allocator(),
                                  IndexMask(attributes.size()),
                                  attributes,
                                  {BLI::get_class_id<ParticleCurrentTimesContext>()},
                                  {(void *)&current_times_context});
    this->execute(context);
  }
}

void ParticleAction::execute_from_event(EventExecuteInterface &event_interface)
{
  ParticleCurrentTimesContext current_times_context;
  current_times_context.current_times = event_interface.current_times();

  ParticleIntegratedOffsets offsets_context = {event_interface.attribute_offsets()};

  ParticleRemainingTimeInStep remaining_time_context;
  remaining_time_context.remaining_times = event_interface.remaining_durations();

  ParticleActionContext context(
      event_interface.particle_allocator(),
      event_interface.pindices(),
      event_interface.attributes(),
      {BLI::get_class_id<ParticleCurrentTimesContext>(),
       BLI::get_class_id<ParticleIntegratedOffsets>(),
       BLI::get_class_id<ParticleRemainingTimeInStep>()},
      {(void *)&current_times_context, (void *)&offsets_context, (void *)&remaining_time_context});
  this->execute(context);
}

void ParticleAction::execute_for_subset(IndexMask mask, ParticleActionContext &parent_context)
{
  ParticleActionContext context(parent_context.particle_allocator(),
                                mask,
                                parent_context.attributes(),
                                parent_context.custom_context_ids(),
                                parent_context.custom_contexts());
  this->execute(context);
}

void ParticleAction::execute_from_offset_handler(OffsetHandlerInterface &offset_handler_interface)
{
  LargeScopedArray<float> current_times(offset_handler_interface.array_size());
  for (uint pindex : offset_handler_interface.pindices()) {
    current_times[pindex] = offset_handler_interface.time_span(pindex).start();
  }

  ParticleCurrentTimesContext current_times_context;
  current_times_context.current_times = current_times;

  ParticleIntegratedOffsets offsets_context = {offset_handler_interface.attribute_offsets()};

  ParticleRemainingTimeInStep remaining_time_context;
  remaining_time_context.remaining_times = offset_handler_interface.remaining_durations();

  ParticleActionContext context(
      offset_handler_interface.particle_allocator(),
      offset_handler_interface.pindices(),
      offset_handler_interface.attributes(),
      {BLI::get_class_id<ParticleCurrentTimesContext>(),
       BLI::get_class_id<ParticleIntegratedOffsets>(),
       BLI::get_class_id<ParticleRemainingTimeInStep>()},
      {(void *)&current_times_context, (void *)&offsets_context, (void *)&remaining_time_context});
  this->execute(context);
}

}  // namespace BParticles
