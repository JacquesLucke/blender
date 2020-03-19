#pragma once

#include "BLI_index_mask.h"
#include "BLI_static_class_ids.h"

#include "emitter_interface.hpp"
#include "event_interface.hpp"
#include "offset_handler_interface.hpp"
#include "particle_allocator.hpp"

namespace BParticles {

using BLI::IndexMask;

class ParticleActionContext {
 private:
  ParticleAllocator &m_particle_allocator;
  IndexMask m_mask;
  MutableAttributesRef m_attributes;
  BufferCache &m_buffer_cache;

  ArrayRef<BLI::class_id_t> m_custom_context_ids;
  ArrayRef<void *> m_custom_contexts;

 public:
  ParticleActionContext(ParticleAllocator &particle_allocator,
                        IndexMask mask,
                        MutableAttributesRef attributes,
                        BufferCache &buffer_cache,
                        ArrayRef<BLI::class_id_t> custom_context_ids,
                        ArrayRef<void *> custom_contexts)
      : m_particle_allocator(particle_allocator),
        m_mask(mask),
        m_attributes(attributes),
        m_buffer_cache(buffer_cache),
        m_custom_context_ids(custom_context_ids),
        m_custom_contexts(custom_contexts)
  {
    BLI::assert_same_size(m_custom_context_ids, m_custom_contexts);
  }

  ArrayRef<BLI::class_id_t> custom_context_ids() const
  {
    return m_custom_context_ids;
  }

  ArrayRef<void *> custom_contexts() const
  {
    return m_custom_contexts;
  }

  ParticleAllocator &particle_allocator()
  {
    return m_particle_allocator;
  }

  IndexMask mask() const
  {
    return m_mask;
  }

  MutableAttributesRef attributes()
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

  BufferCache &buffer_cache()
  {
    return m_buffer_cache;
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
  void execute_for_subset(IndexMask mask, ParticleActionContext &parent_context);
  void execute_from_offset_handler(OffsetHandlerInterface &offset_handler_interface);
};

struct ParticleCurrentTimesContext {
  ArrayRef<float> current_times;
};

struct ParticleIntegratedOffsets {
  MutableAttributesRef offsets;
};

struct ParticleRemainingTimeInStep {
  ArrayRef<float> remaining_times;
};

}  // namespace BParticles
