#pragma once

#include "BLI_static_class_ids.h"
#include "BLI_index_mask.h"

#include "particle_allocator.hpp"
#include "emitter_interface.hpp"
#include "event_interface.hpp"
#include "offset_handler_interface.hpp"

namespace BParticles {

using BLI::IndexMask;

class ParticleActionContext {
 private:
  ParticleAllocator &m_particle_allocator;
  IndexMask m_pindex_mask;
  AttributesRef m_attributes;

  ArrayRef<BLI::class_id_t> m_custom_context_ids;
  ArrayRef<void *> m_custom_contexts;

 public:
  ParticleActionContext(ParticleAllocator &particle_allocator,
                        IndexMask pindex_mask,
                        AttributesRef attributes,
                        ArrayRef<BLI::class_id_t> custom_context_ids,
                        ArrayRef<void *> custom_contexts)
      : m_particle_allocator(particle_allocator),
        m_pindex_mask(pindex_mask),
        m_attributes(attributes),
        m_custom_context_ids(custom_context_ids),
        m_custom_contexts(custom_contexts)
  {
    BLI_assert(m_custom_context_ids.size() == m_custom_contexts.size());
  }

  ParticleAllocator &particle_allocator()
  {
    return m_particle_allocator;
  }

  IndexMask pindex_mask() const
  {
    return m_pindex_mask;
  }

  AttributesRef attributes()
  {
    return m_attributes;
  }

  template<typename T> T *try_find() const
  {
    BLI::class_id_t context_id = BLI::get_class_id<T>();
    int index = m_custom_context_ids.first_index_try(context_id);
    if (index >= 0) {
      return reinterpret_cast<T *>(m_custom_contexts[index]);
    }
    else {
      return nullptr;
    }
  }
};

class ParticleAction {
 public:
  virtual ~ParticleAction();

  virtual void execute(ParticleActionContext &context) = 0;

  void execute_from_emitter(AttributesRefGroup &new_particles,
                            EmitterInterface &emitter_interface);
  void execute_for_new_particles(AttributesRefGroup &new_particles,
                                 ParticleActionContext &parent_context);
  void execute_for_new_particles(AttributesRefGroup &new_particles,
                                 OffsetHandlerInterface &offset_handler_interface);
  void execute_from_event(EventExecuteInterface &event_interface);
  void execute_for_subset(IndexMask pindex_mask, ParticleActionContext &parent_context);
  void execute_from_offset_handler(OffsetHandlerInterface &offset_handler_interface);
};

inline void ParticleAction::execute_from_emitter(AttributesRefGroup &new_particles,
                                                 EmitterInterface &emitter_interface)
{
  for (AttributesRef attributes : new_particles) {
    ParticleActionContext context(
        emitter_interface.particle_allocator(), IndexMask(attributes.size()), attributes, {}, {});
    this->execute(context);
  }
}

inline void ParticleAction::execute_for_new_particles(AttributesRefGroup &new_particles,
                                                      ParticleActionContext &parent_context)
{
  for (AttributesRef attributes : new_particles) {
    ParticleActionContext context(
        parent_context.particle_allocator(), IndexMask(attributes.size()), attributes, {}, {});
    this->execute(context);
  }
}

inline void ParticleAction::execute_for_new_particles(
    AttributesRefGroup &new_particles, OffsetHandlerInterface &offset_handler_interface)
{
  for (AttributesRef attributes : new_particles) {
    ParticleActionContext context(offset_handler_interface.particle_allocator(),
                                  IndexMask(attributes.size()),
                                  attributes,
                                  {},
                                  {});
    this->execute(context);
  }
}

inline void ParticleAction::execute_from_event(EventExecuteInterface &event_interface)
{
  ParticleActionContext context(event_interface.particle_allocator(),
                                event_interface.pindices(),
                                event_interface.attributes(),
                                {},
                                {});
  this->execute(context);
}

inline void ParticleAction::execute_for_subset(IndexMask pindex_mask,
                                               ParticleActionContext &parent_context)
{
  ParticleActionContext context(
      parent_context.particle_allocator(), pindex_mask, parent_context.attributes(), {}, {});
  this->execute(context);
}

inline void ParticleAction::execute_from_offset_handler(
    OffsetHandlerInterface &offset_handler_interface)
{
  ParticleActionContext context(offset_handler_interface.particle_allocator(),
                                offset_handler_interface.pindices(),
                                offset_handler_interface.attributes(),
                                {},
                                {});
  this->execute(context);
}

}  // namespace BParticles
