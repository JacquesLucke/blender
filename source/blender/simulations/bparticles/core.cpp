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

ParticleType::~ParticleType()
{
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

BlockAllocator::BlockAllocator(ParticlesState &state) : m_state(state)
{
}

ParticlesBlock &BlockAllocator::get_non_full_block(StringRef particle_type_name)
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

void BlockAllocator::allocate_block_ranges(StringRef particle_type_name,
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

AttributesInfo &BlockAllocator::attributes_info(StringRef particle_type_name)
{
  return m_state.particle_container(particle_type_name).attributes_info();
}

/* Emitter Interface
 ******************************************/

EmitterInterface::EmitterInterface(BlockAllocator &allocator, TimeSpan time_span)
    : m_block_allocator(allocator), m_time_span(time_span)
{
}

EmitterInterface::~EmitterInterface()
{
  for (TimeSpanEmitTarget *target : m_targets) {
    delete target;
  }
}

TimeSpanEmitTarget &EmitterInterface::request(StringRef particle_type_name, uint size)
{
  SmallVector<ParticlesBlock *> blocks;
  SmallVector<Range<uint>> ranges;
  m_block_allocator.allocate_block_ranges(particle_type_name, size, blocks, ranges);
  AttributesInfo &attributes_info = m_block_allocator.attributes_info(particle_type_name);

  auto *target = new TimeSpanEmitTarget(
      particle_type_name, attributes_info, blocks, ranges, m_time_span);
  m_targets.append(target);
  return *target;
}

/* Action Interface
 **************************************/

EventExecuteInterface::~EventExecuteInterface()
{
  for (InstantEmitTarget *target : m_emit_targets) {
    delete target;
  }
}

InstantEmitTarget &EventExecuteInterface::request_emit_target(StringRef particle_type_name,
                                                              ArrayRef<uint> original_indices)
{
  uint size = original_indices.size();

  SmallVector<ParticlesBlock *> blocks;
  SmallVector<Range<uint>> ranges;
  m_block_allocator.allocate_block_ranges(particle_type_name, size, blocks, ranges);
  AttributesInfo &attributes_info = m_block_allocator.attributes_info(particle_type_name);

  auto *target = new InstantEmitTarget(particle_type_name, attributes_info, blocks, ranges);
  m_emit_targets.append(target);

  SmallVector<float> birth_times(size);
  for (uint i = 0; i < size; i++) {
    birth_times[i] = m_current_times[original_indices[i]];
  }
  target->set_float("Birth Time", birth_times);

  return *target;
}

/* EmitTarget
 ******************************************/

EmitTargetBase::EmitTargetBase(StringRef particle_type_name,
                               AttributesInfo &attributes_info,
                               ArrayRef<ParticlesBlock *> blocks,
                               ArrayRef<Range<uint>> ranges)
    : m_particle_type_name(particle_type_name.to_std_string()),
      m_attributes_info(attributes_info),
      m_blocks(blocks),
      m_ranges(ranges)
{
  BLI_assert(blocks.size() == ranges.size());
  for (auto range : ranges) {
    m_size += range.size();
  }
}

InstantEmitTarget::InstantEmitTarget(StringRef particle_type_name,
                                     AttributesInfo &attributes_info,
                                     ArrayRef<ParticlesBlock *> blocks,
                                     ArrayRef<Range<uint>> ranges)
    : EmitTargetBase(particle_type_name, attributes_info, blocks, ranges)
{
}

TimeSpanEmitTarget::TimeSpanEmitTarget(StringRef particle_type_name,
                                       AttributesInfo &attributes_info,
                                       ArrayRef<ParticlesBlock *> blocks,
                                       ArrayRef<Range<uint>> ranges,
                                       TimeSpan time_span)
    : EmitTargetBase(particle_type_name, attributes_info, blocks, ranges), m_time_span(time_span)
{
}

void EmitTargetBase::set_elements(uint index, void *data)
{
  AttributeType type = m_attributes_info.type_of(index);
  uint element_size = size_of_attribute_type(type);

  void *remaining_data = data;

  for (uint part = 0; part < m_ranges.size(); part++) {
    ParticlesBlock &block = *m_blocks[part];
    Range<uint> range = m_ranges[part];

    AttributeArrays attributes = block.attributes_slice(range);
    void *dst = attributes.get_ptr(index);
    uint bytes_to_copy = element_size * attributes.size();
    memcpy(dst, remaining_data, bytes_to_copy);

    remaining_data = POINTER_OFFSET(remaining_data, bytes_to_copy);
  }
}

void EmitTargetBase::fill_elements(uint index, void *value)
{
  AttributeType type = m_attributes_info.type_of(index);
  uint element_size = size_of_attribute_type(type);

  for (uint part = 0; part < m_ranges.size(); part++) {
    ParticlesBlock &block = *m_blocks[part];

    /* TODO(jacques): Check if this is correct. */
    void *dst = block.attributes_all().get_ptr(index);
    for (uint i : m_ranges[part]) {
      memcpy(POINTER_OFFSET(dst, element_size * i), value, element_size);
    }
  }
}

void EmitTargetBase::set_byte(uint index, ArrayRef<uint8_t> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Byte);
  this->set_elements(index, (void *)data.begin());
}
void EmitTargetBase::set_float(uint index, ArrayRef<float> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Float);
  this->set_elements(index, (void *)data.begin());
}
void EmitTargetBase::set_float3(uint index, ArrayRef<float3> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Float3);
  this->set_elements(index, (void *)data.begin());
}

void EmitTargetBase::set_byte(StringRef name, ArrayRef<uint8_t> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_byte(index, data);
}
void EmitTargetBase::set_float(StringRef name, ArrayRef<float> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_float(index, data);
}
void EmitTargetBase::set_float3(StringRef name, ArrayRef<float3> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_float3(index, data);
}

void EmitTargetBase::fill_byte(uint index, uint8_t value)
{
  this->fill_elements(index, (void *)&value);
}

void EmitTargetBase::fill_byte(StringRef name, uint8_t value)
{
  uint index = m_attributes_info.attribute_index(name);
  this->fill_byte(index, value);
}

void EmitTargetBase::fill_float(uint index, float value)
{
  this->fill_elements(index, (void *)&value);
}

void EmitTargetBase::fill_float(StringRef name, float value)
{
  uint index = m_attributes_info.attribute_index(name);
  this->fill_float(index, value);
}

void EmitTargetBase::fill_float3(uint index, float3 value)
{
  this->fill_elements(index, (void *)&value);
}

void EmitTargetBase::fill_float3(StringRef name, float3 value)
{
  uint index = m_attributes_info.attribute_index(name);
  this->fill_float3(index, value);
}

void TimeSpanEmitTarget::set_birth_moment(float time_factor)
{
  BLI_assert(time_factor >= 0.0 && time_factor <= 1.0f);
  this->fill_float("Birth Time", m_time_span.interpolate(time_factor));
}

void TimeSpanEmitTarget::set_randomized_birth_moments()
{
  SmallVector<float> birth_times(m_size);
  for (uint i = 0; i < m_size; i++) {
    float factor = (rand() % 10000) / 10000.0f;
    birth_times[i] = m_time_span.interpolate(factor);
  }
  this->set_float("Birth Time", birth_times);
}

void TimeSpanEmitTarget::set_birth_moments(ArrayRef<float> time_factors)
{
  BLI_assert(time_factors.size() == m_size);
  SmallVector<float> birth_times(time_factors.size());
  for (uint i = 0; i < m_size; i++) {
    birth_times[i] = m_time_span.interpolate(time_factors[i]);
  }
  this->set_float("Birth Time", birth_times);
}

/* EventFilterInterface
 *****************************************/

EventFilterInterface::EventFilterInterface(ParticleSet particles,
                                           AttributeArrays &attribute_offsets,
                                           ArrayRef<float> durations,
                                           float end_time,
                                           ArrayRef<float> known_min_time_factors,
                                           EventStorage &r_event_storage,
                                           SmallVector<uint> &r_filtered_indices,
                                           SmallVector<float> &r_filtered_time_factors)
    : m_particles(particles),
      m_attribute_offsets(attribute_offsets),
      m_durations(durations),
      m_end_time(end_time),
      m_known_min_time_factors(known_min_time_factors),
      m_event_storage(r_event_storage),
      m_filtered_indices(r_filtered_indices),
      m_filtered_time_factors(r_filtered_time_factors)
{
}

/* EventExecuteInterface
 *************************************************/

EventExecuteInterface::EventExecuteInterface(ParticleSet particles,
                                             BlockAllocator &block_allocator,
                                             ArrayRef<float> current_times,
                                             EventStorage &event_storage,
                                             AttributeArrays attribute_offsets,
                                             float step_end_time)
    : m_particles(particles),
      m_block_allocator(block_allocator),
      m_current_times(current_times),
      m_kill_states(m_particles.attributes().get_byte("Kill State")),
      m_event_storage(event_storage),
      m_attribute_offsets(attribute_offsets),
      m_step_end_time(step_end_time)
{
}

/* IntegratorInterface
 ***************************************************/

IntegratorInterface::IntegratorInterface(ParticlesBlock &block,
                                         ArrayRef<float> durations,
                                         AttributeArrays r_offsets)
    : m_block(block), m_durations(durations), m_offsets(r_offsets)
{
}

}  // namespace BParticles
