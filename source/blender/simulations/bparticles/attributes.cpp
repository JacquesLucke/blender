#include "attributes.hpp"

namespace BParticles {

AttributesInfo::AttributesInfo(ArrayRef<std::string> byte_names,
                               ArrayRef<std::string> float_names,
                               ArrayRef<std::string> float3_names)
{
  m_indices = {};
  m_indices.add_multiple(byte_names);
  m_indices.add_multiple(float_names);
  m_indices.add_multiple(float3_names);
  BLI_assert(m_indices.size() == byte_names.size() + float_names.size() + float3_names.size());

  m_byte_attributes = Range<uint>(0, byte_names.size());
  m_float_attributes = m_byte_attributes.after(float_names.size());
  m_float3_attributes = m_float_attributes.after(float3_names.size());

  m_types = {};
  m_types.append_n_times(AttributeType::Byte, m_byte_attributes.size());
  m_types.append_n_times(AttributeType::Float, m_float_attributes.size());
  m_types.append_n_times(AttributeType::Float3, m_float3_attributes.size());
}

SmallVector<void *> AttributesInfo::allocate_separate_arrays(uint size) const
{
  SmallVector<void *> pointers;
  for (AttributeType type : m_types) {
    pointers.append(MEM_malloc_arrayN(size, size_of_attribute_type(type), __func__));
  }
  return pointers;
}

void JoinedAttributeArrays::set_elements(uint index, void *data)
{
  AttributeType type = m_info.type_of(index);
  uint element_size = size_of_attribute_type(type);

  void *remaining_data = data;

  for (auto arrays : m_arrays) {
    void *target = arrays.get_ptr(index);
    uint bytes_to_copy = element_size * arrays.size();
    memcpy(target, remaining_data, bytes_to_copy);

    remaining_data = POINTER_OFFSET(remaining_data, bytes_to_copy);
  }
}

void JoinedAttributeArrays::set_byte(uint index, ArrayRef<uint8_t> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_info.type_of(index) == AttributeType::Byte);
  this->set_elements(index, (void *)data.begin());
}
void JoinedAttributeArrays::set_float(uint index, ArrayRef<float> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_info.type_of(index) == AttributeType::Float);
  this->set_elements(index, (void *)data.begin());
}
void JoinedAttributeArrays::set_float3(uint index, ArrayRef<float3> data)
{
  BLI_assert(data.size() == m_size);
  BLI_assert(m_info.type_of(index) == AttributeType::Float3);
  this->set_elements(index, (void *)data.begin());
}

void JoinedAttributeArrays::set_byte(StringRef name, ArrayRef<uint8_t> data)
{
  uint index = m_info.attribute_index(name);
  this->set_byte(index, data);
}
void JoinedAttributeArrays::set_float(StringRef name, ArrayRef<float> data)
{
  uint index = m_info.attribute_index(name);
  this->set_float(index, data);
}
void JoinedAttributeArrays::set_float3(StringRef name, ArrayRef<float3> data)
{
  uint index = m_info.attribute_index(name);
  this->set_float3(index, data);
}

};  // namespace BParticles
