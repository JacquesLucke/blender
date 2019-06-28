#include "core.hpp"

namespace BParticles {

Force::~Force()
{
}

Emitter::~Emitter()
{
}

Action::~Action()
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

ParticlesBlock &BlockAllocator::get_non_full_block(uint particle_type_id)
{
  ParticlesContainer &container = m_state.particle_container(particle_type_id);

  uint index = 0;
  while (index < m_non_full_cache.size()) {
    if (m_non_full_cache[index]->inactive_amount() == 0) {
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

/* Emitter Interface
 ******************************************/

EmitterInterface::~EmitterInterface()
{
  for (EmitTarget *target : m_targets) {
    delete target;
  }
}

EmitTarget &EmitterInterface::request(uint particle_type_id, uint size)
{
  SmallVector<ParticlesBlock *> blocks;
  SmallVector<Range<uint>> ranges;

  uint remaining_size = size;
  while (remaining_size > 0) {
    ParticlesBlock &block = m_block_allocator.get_non_full_block(particle_type_id);

    uint size_to_use = std::min(block.inactive_amount(), remaining_size);
    Range<uint> range(block.active_amount(), block.active_amount() + size_to_use);
    block.active_amount() += size_to_use;

    blocks.append(&block);
    ranges.append(range);

    AttributeArrays attributes = block.slice(range);
    for (uint i : attributes.info().attribute_indices()) {
      attributes.init_default(i);
    }

    remaining_size -= size_to_use;
  }

  ParticlesContainer &container = m_block_allocator.particles_state().particle_container(
      particle_type_id);
  m_targets.append(new EmitTarget(particle_type_id, container.attributes_info(), blocks, ranges));
  return *m_targets.last();
}

/* EmitTarget
 ******************************************/

void EmitTarget::set_elements(uint index, void *data)
{
  AttributeType type = m_attributes_info.type_of(index);
  uint element_size = size_of_attribute_type(type);

  void *remaining_data = data;

  for (uint part = 0; part < m_ranges.size(); part++) {
    ParticlesBlock &block = *m_blocks[part];
    Range<uint> range = m_ranges[part];

    AttributeArrays attributes = block.slice(range);
    void *dst = attributes.get_ptr(index);
    uint bytes_to_copy = element_size * attributes.size();
    memcpy(dst, remaining_data, bytes_to_copy);

    remaining_data = POINTER_OFFSET(remaining_data, bytes_to_copy);
  }
}

void EmitTarget::fill_elements(uint index, void *value)
{
  AttributeType type = m_attributes_info.type_of(index);
  uint element_size = size_of_attribute_type(type);

  for (uint part = 0; part < m_ranges.size(); part++) {
    ParticlesBlock &block = *m_blocks[part];

    void *dst = block.slice_all().get_ptr(index);
    for (uint i : m_ranges[part]) {
      memcpy(POINTER_OFFSET(dst, element_size * i), value, element_size);
    }
  }
}

void EmitTarget::set_byte(uint index, ArrayRef<uint8_t> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Byte);
  this->set_elements(index, (void *)data.begin());
}
void EmitTarget::set_float(uint index, ArrayRef<float> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Float);
  this->set_elements(index, (void *)data.begin());
}
void EmitTarget::set_float3(uint index, ArrayRef<float3> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_attributes_info.type_of(index) == AttributeType::Float3);
  this->set_elements(index, (void *)data.begin());
}

void EmitTarget::set_byte(StringRef name, ArrayRef<uint8_t> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_byte(index, data);
}
void EmitTarget::set_float(StringRef name, ArrayRef<float> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_float(index, data);
}
void EmitTarget::set_float3(StringRef name, ArrayRef<float3> data)
{
  uint index = m_attributes_info.attribute_index(name);
  this->set_float3(index, data);
}

void EmitTarget::fill_byte(uint index, uint8_t value)
{
  this->fill_elements(index, (void *)&value);
}

void EmitTarget::fill_byte(StringRef name, uint8_t value)
{
  uint index = m_attributes_info.attribute_index(name);
  this->fill_byte(index, value);
}

void EmitTarget::fill_float(uint index, float value)
{
  this->fill_elements(index, (void *)&value);
}

void EmitTarget::fill_float(StringRef name, float value)
{
  uint index = m_attributes_info.attribute_index(name);
  this->fill_float(index, value);
}

void EmitTarget::fill_float3(uint index, float3 value)
{
  this->fill_elements(index, (void *)&value);
}

void EmitTarget::fill_float3(StringRef name, float3 value)
{
  uint index = m_attributes_info.attribute_index(name);
  this->fill_float3(index, value);
}

void EmitTarget::set_birth_moment(float time_factor)
{
  BLI_assert(time_factor >= 0.0 && time_factor <= 1.0f);
  m_birth_moments.fill(time_factor);
}

void EmitTarget::set_randomized_birth_moments()
{
  for (float &birth_moment : m_birth_moments) {
    birth_moment = (rand() % 10000) / 10000.0f;
  }
}

}  // namespace BParticles
