#include "attributes.hpp"

namespace BParticles {

AttributesInfo::AttributesInfo(AttributesDeclaration &builder)
    : AttributesInfo(builder.m_byte_names,
                     builder.m_float_names,
                     builder.m_float3_names,
                     builder.m_byte_defaults,
                     builder.m_float_defaults,
                     builder.m_float3_defaults)
{
}

AttributesInfo::AttributesInfo(ArrayRef<std::string> byte_names,
                               ArrayRef<std::string> float_names,
                               ArrayRef<std::string> float3_names,
                               ArrayRef<uint8_t> byte_defaults,
                               ArrayRef<float> float_defaults,
                               ArrayRef<float3> float3_defaults)
{
  BLI_assert(byte_names.size() == byte_defaults.size());
  BLI_assert(float_names.size() == float_defaults.size());
  BLI_assert(float3_names.size() == float3_defaults.size());

  m_indices = {};
  m_indices.add_multiple_new(byte_names);
  m_indices.add_multiple_new(float_names);
  m_indices.add_multiple_new(float3_names);
  BLI_assert(m_indices.size() == byte_names.size() + float_names.size() + float3_names.size());

  m_byte_attributes = Range<uint>(0, byte_names.size());
  m_float_attributes = m_byte_attributes.after(float_names.size());
  m_float3_attributes = m_float_attributes.after(float3_names.size());

  m_types = {};
  m_types.append_n_times(AttributeType::Byte, m_byte_attributes.size());
  m_types.append_n_times(AttributeType::Float, m_float_attributes.size());
  m_types.append_n_times(AttributeType::Float3, m_float3_attributes.size());

  m_byte_defaults = byte_defaults;
  m_float_defaults = float_defaults;
  m_float3_defaults = float3_defaults;
}

AttributeArraysCore::AttributeArraysCore(AttributesInfo &info, ArrayRef<void *> arrays, uint size)
    : m_info(&info), m_arrays(arrays), m_size(size)
{
}

AttributeArraysCore::~AttributeArraysCore()
{
}

AttributeArraysCore AttributeArraysCore::NewWithSeparateAllocations(AttributesInfo &info,
                                                                    uint size)
{
  SmallVector<void *> arrays;
  for (AttributeType type : info.types()) {
    uint bytes_size = size * size_of_attribute_type(type);
    void *ptr = MEM_mallocN_aligned(bytes_size, 64, __func__);
    arrays.append(ptr);
  }
  return AttributeArraysCore(info, arrays, size);
}

AttributeArraysCore AttributeArraysCore::NewWithArrayAllocator(AttributesInfo &info,
                                                               ArrayAllocator &allocator)
{
  SmallVector<void *> arrays;
  for (AttributeType type : info.types()) {
    uint element_size = size_of_attribute_type(type);
    void *ptr = allocator.allocate(element_size);
    arrays.append(ptr);
  }
  return AttributeArraysCore(info, arrays, allocator.array_size());
}

void AttributeArraysCore::free_buffers()
{
  for (void *ptr : m_arrays) {
    MEM_freeN(ptr);
  }
}

void AttributeArraysCore::deallocate_in_array_allocator(ArrayAllocator &allocator)
{
  for (uint i = 0; i < m_arrays.size(); i++) {
    void *ptr = m_arrays[i];
    uint element_size = size_of_attribute_type(m_info->type_of(i));
    allocator.deallocate(ptr, element_size);
  }
}

};  // namespace BParticles
