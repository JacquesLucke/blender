#include "core.hpp"

namespace BParticles {

Emitter::~Emitter()
{
}

Integrator::~Integrator()
{
}

Event::~Event()
{
}

void Event::attributes(AttributesDeclaration &UNUSED(interface))
{
}

OffsetHandler::~OffsetHandler()
{
}

ParticleType::~ParticleType()
{
  delete m_integrator;

  for (Event *event : m_events) {
    delete event;
  }
  for (OffsetHandler *handler : m_offset_handlers) {
    delete handler;
  }
}

StepDescription::~StepDescription()
{
  for (auto *type : m_types.values()) {
    delete type;
  }
  for (Emitter *emitter : m_emitters) {
    delete emitter;
  }
}

ParticlesState::~ParticlesState()
{
  for (ParticlesContainer *container : m_container_by_id.values()) {
    delete container;
  }
}

/* Block Allocator
 ******************************************/

ParticleAllocator::ParticleAllocator(ParticlesState &state) : m_state(state)
{
}

ParticlesBlock &ParticleAllocator::get_non_full_block(StringRef particle_type_name)
{
  ParticlesContainer &container = m_state.particle_container(particle_type_name);

  uint index = 0;
  while (index < m_non_full_cache.size()) {
    if (m_non_full_cache[index]->unused_amount() == 0) {
      m_non_full_cache.remove_and_reorder(index);
      continue;
    }

    if (m_non_full_cache[index]->container() == container) {
      return *m_non_full_cache[index];
    }
    index++;
  }

  ParticlesBlock &block = container.new_block();
  m_non_full_cache.append(&block);
  m_allocated_blocks.append(&block);
  return block;
}

void ParticleAllocator::allocate_block_ranges(StringRef particle_type_name,
                                              uint size,
                                              SmallVector<ParticlesBlock *> &r_blocks,
                                              SmallVector<Range<uint>> &r_ranges)
{
  uint remaining_size = size;
  while (remaining_size > 0) {
    ParticlesBlock &block = this->get_non_full_block(particle_type_name);

    uint size_to_use = std::min(block.unused_amount(), remaining_size);
    Range<uint> range(block.active_amount(), block.active_amount() + size_to_use);
    block.active_amount() += size_to_use;

    r_blocks.append(&block);
    r_ranges.append(range);

    AttributeArrays attributes = block.attributes_slice(range);
    for (uint i : attributes.info().attribute_indices()) {
      attributes.init_default(i);
    }

    remaining_size -= size_to_use;
  }
}

AttributesInfo &ParticleAllocator::attributes_info(StringRef particle_type_name)
{
  return m_state.particle_container(particle_type_name).attributes_info();
}

ParticleSets ParticleAllocator::request(StringRef particle_type_name, uint size)
{
  SmallVector<ParticlesBlock *> blocks;
  SmallVector<Range<uint>> ranges;
  this->allocate_block_ranges(particle_type_name, size, blocks, ranges);

  AttributesInfo &attributes_info = this->attributes_info(particle_type_name);

  SmallVector<ParticleSet> sets;
  for (uint i = 0; i < blocks.size(); i++) {
    sets.append(ParticleSet(*blocks[i], ranges[i].as_array_ref()));
  }

  return ParticleSets(particle_type_name, attributes_info, sets);
}

/* Emitter Interface
 ******************************************/

EmitterInterface::EmitterInterface(ParticleAllocator &particle_allocator,
                                   ArrayAllocator &array_allocator,
                                   TimeSpan time_span)
    : m_particle_allocator(particle_allocator),
      m_array_allocator(array_allocator),
      m_time_span(time_span)
{
}

/* EventFilterInterface
 *****************************************/

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

/* EventExecuteInterface
 *************************************************/

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

/* IntegratorInterface
 ***************************************************/

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

/* OffsetHandlerInterface
 ****************************************************/

OffsetHandlerInterface::OffsetHandlerInterface(BlockStepData &step_data,
                                               ArrayRef<uint> pindices,
                                               ArrayRef<float> time_factors)
    : m_step_data(step_data), m_pindices(pindices), m_time_factors(time_factors)
{
}

}  // namespace BParticles
