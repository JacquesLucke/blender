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
  for (ParticlesContainer *container : m_particle_containers.values()) {
    delete container;
  }
}

/* EmitTarget
 ******************************************/

void EmitTarget::set_elements(uint index, void *data)
{
  AttributeType type = m_attributes_info.type_of(index);
  uint element_size = size_of_attribute_type(type);

  void *remaining_data = data;

  for (uint i = 0; i < m_ranges.size(); i++) {
    ParticlesBlock &block = *m_blocks[i];
    Range<uint> range = m_ranges[i];

    AttributeArrays attributes = block.slice(range);
    void *dst = attributes.get_ptr(index);
    uint bytes_to_copy = element_size * attributes.size();
    memcpy(dst, remaining_data, bytes_to_copy);

    remaining_data = POINTER_OFFSET(remaining_data, bytes_to_copy);
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

}  // namespace BParticles
