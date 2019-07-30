#include "attributes.hpp"

namespace BParticles {

void AttributesDeclaration::join(AttributesDeclaration &other)
{
  for (uint i = 0; i < other.size(); i++) {
    if (m_names.add(other.m_names[i])) {
      m_types.append(other.m_types[i]);
      m_defaults.append(other.m_defaults[i]);
    }
  }
}

void AttributesDeclaration::join(AttributesInfo &other)
{
  for (uint i = 0; i < other.size(); i++) {
    if (m_names.add(other.m_names[i])) {
      m_types.append(other.m_types[i]);
      m_defaults.append(other.m_defaults[i]);
    }
  }
}

AttributesInfo::AttributesInfo(AttributesDeclaration &builder)
    : m_names(builder.m_names), m_types(builder.m_types), m_defaults(builder.m_defaults)
{
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
  Vector<void *> arrays;
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
  Vector<void *> arrays;
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
