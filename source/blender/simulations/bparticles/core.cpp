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

ForwardingListener::~ForwardingListener()
{
}

ParticleType::~ParticleType()
{
}

ArrayRef<ForwardingListener *> ParticleType::forwarding_listeners()
{
  return {};
}

ArrayRef<Event *> ParticleType::events()
{
  return {};
}

StepDescription::~StepDescription()
{
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

/* ParticleSets
 ******************************************/

ParticleSets::ParticleSets(StringRef particle_type_name,
                           AttributesInfo &attributes_info,
                           ArrayRef<ParticleSet> sets)
    : m_particle_type_name(particle_type_name.to_std_string()),
      m_attributes_info(attributes_info),
      m_sets(sets)
{
  m_size = 0;
  for (auto &set : sets) {
    m_size += set.size();
  }
}

void ParticleSets::set_elements(uint index, void *data)
{
  AttributeType type = m_attributes_info.type_of(index);
  uint element_size = size_of_attribute_type(type);

  void *remaining_data = data;

  for (ParticleSet particles : m_sets) {
    AttributeArrays attributes = particles.attributes();
    void *dst = attributes.get_ptr(index);

    for (uint i = 0; i < particles.size(); i++) {
      uint pindex = particles.pindices()[i];
      memcpy(POINTER_OFFSET(dst, element_size * pindex),
             POINTER_OFFSET(remaining_data, element_size * i),
             element_size);
    }

    remaining_data = POINTER_OFFSET(remaining_data, particles.size() * element_size);
  }
}

void ParticleSets::set_repeated_elements(uint index,
                                         void *data,
                                         uint data_element_amount,
                                         void *default_value)
{
  if (data_element_amount == 0) {
    this->fill_elements(index, default_value);
    return;
  }

  AttributeType type = m_attributes_info.type_of(index);
  uint element_size = size_of_attribute_type(type);
  uint offset = 0;
  for (ParticleSet particles : m_sets) {
    AttributeArrays attributes = particles.attributes();
    void *dst = attributes.get_ptr(index);
    for (uint pindex : particles.pindices()) {
      memcpy(POINTER_OFFSET(dst, element_size * pindex),
             POINTER_OFFSET(data, element_size * offset),
             element_size);
      offset++;
      if (offset == data_element_amount) {
        offset = 0;
      }
    }
  }
}

void ParticleSets::fill_elements(uint index, void *value)
{
  AttributeType type = m_attributes_info.type_of(index);
  uint element_size = size_of_attribute_type(type);

  for (ParticleSet particles : m_sets) {
    AttributeArrays attributes = particles.attributes();
    void *dst = attributes.get_ptr(index);

    for (uint pindex : particles.pindices()) {
      memcpy(POINTER_OFFSET(dst, element_size * pindex), value, element_size);
    }
  }
}

void ParticleSets::set_byte(uint index, ArrayRef<uint8_t> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Byte);
  this->set_elements(index, (void *)data.begin());
}
void ParticleSets::set_float(uint index, ArrayRef<float> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Float);
  this->set_elements(index, (void *)data.begin());
}
void ParticleSets::set_float3(uint index, ArrayRef<float3> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Float3);
  this->set_elements(index, (void *)data.begin());
}

void ParticleSets::set_byte(StringRef name, ArrayRef<uint8_t> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_byte(index, data);
}
void ParticleSets::set_float(StringRef name, ArrayRef<float> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_float(index, data);
}
void ParticleSets::set_float3(StringRef name, ArrayRef<float3> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_float3(index, data);
}

void ParticleSets::set_repeated_byte(uint index, ArrayRef<uint8_t> data)
{
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Byte);
  this->set_repeated_elements(
      index, (void *)data.begin(), data.size(), m_attributes_info.default_value_ptr(index));
}
void ParticleSets::set_repeated_float(uint index, ArrayRef<float> data)
{
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Float);
  this->set_repeated_elements(
      index, (void *)data.begin(), data.size(), m_attributes_info.default_value_ptr(index));
}
void ParticleSets::set_repeated_float3(uint index, ArrayRef<float3> data)
{
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Float3);
  this->set_repeated_elements(
      index, (void *)data.begin(), data.size(), m_attributes_info.default_value_ptr(index));
}

void ParticleSets::set_repeated_byte(StringRef name, ArrayRef<uint8_t> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_repeated_byte(index, data);
}
void ParticleSets::set_repeated_float(StringRef name, ArrayRef<float> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_repeated_float(index, data);
}
void ParticleSets::set_repeated_float3(StringRef name, ArrayRef<float3> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_repeated_float3(index, data);
}

void ParticleSets::fill_byte(uint index, uint8_t value)
{
  this->fill_elements(index, (void *)&value);
}

void ParticleSets::fill_byte(StringRef name, uint8_t value)
{
  uint index = m_attributes_info.attribute_index(name);
  this->fill_byte(index, value);
}

void ParticleSets::fill_float(uint index, float value)
{
  this->fill_elements(index, (void *)&value);
}

void ParticleSets::fill_float(StringRef name, float value)
{
  uint index = m_attributes_info.attribute_index(name);
  this->fill_float(index, value);
}

void ParticleSets::fill_float3(uint index, float3 value)
{
  this->fill_elements(index, (void *)&value);
}

void ParticleSets::fill_float3(StringRef name, float3 value)
{
  uint index = m_attributes_info.attribute_index(name);
  this->fill_float3(index, value);
}

/* EventFilterInterface
 *****************************************/

EventFilterInterface::EventFilterInterface(BlockStepData &step_data,
                                           ParticleSet particles,
                                           ArrayRef<float> known_min_time_factors,
                                           EventStorage &r_event_storage,
                                           SmallVector<uint> &r_filtered_pindices,
                                           SmallVector<float> &r_filtered_time_factors)
    : m_step_data(step_data),
      m_particles(particles),
      m_known_min_time_factors(known_min_time_factors),
      m_event_storage(r_event_storage),
      m_filtered_pindices(r_filtered_pindices),
      m_filtered_time_factors(r_filtered_time_factors)
{
}

/* EventExecuteInterface
 *************************************************/

EventExecuteInterface::EventExecuteInterface(BlockStepData &step_data,
                                             ParticleSet particles,
                                             ArrayRef<float> current_times,
                                             EventStorage &event_storage)
    : m_step_data(step_data),
      m_particles(particles),
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

/* ForwardingListenerInterface
 ****************************************************/

ForwardingListenerInterface::ForwardingListenerInterface(BlockStepData &step_data,
                                                         ParticleSet &particles,
                                                         ArrayRef<float> time_factors)
    : m_step_data(step_data), m_particles(particles), m_time_factors(time_factors)
{
}

}  // namespace BParticles
