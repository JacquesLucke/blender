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

};  // namespace BParticles
