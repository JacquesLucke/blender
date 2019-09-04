#include "BKE_attributes_ref.hpp"

namespace BKE {

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
    if (m_names.add(other.m_name_by_index[i])) {
      m_types.append(other.m_type_by_index[i]);
      m_defaults.append(other.m_default_by_index[i]);
    }
  }
}

AttributesInfo::AttributesInfo(AttributesDeclaration &builder)
    : m_name_by_index(builder.m_names),
      m_type_by_index(builder.m_types),
      m_default_by_index(builder.m_defaults)
{
  for (int i = 0; i < m_name_by_index.size(); i++) {
    m_index_by_name.add_new(m_name_by_index[i], i);
  }
}

AttributesRefGroup::AttributesRefGroup(AttributesInfo &attributes_info,
                                       Vector<ArrayRef<void *>> buffers,
                                       Vector<Range<uint>> ranges)
    : m_attributes_info(&attributes_info),
      m_buffers(std::move(buffers)),
      m_ranges(std::move(ranges))
{
  BLI_assert(buffers.size() == ranges.size());
  m_size = 0;
  for (Range<uint> range : m_ranges) {
    m_size += range.size();
  }
}

void AttributesRefGroup::set_elements(uint index, void *data)
{
  AttributeType type = m_attributes_info->type_of(index);
  uint element_size = size_of_attribute_type(type);

  void *remaining_data = data;

  for (AttributesRef attributes : *this) {
    void *dst = attributes.get_ptr(index);

    uint size = attributes.size();
    for (uint pindex = 0; pindex < size; pindex++) {
      memcpy(POINTER_OFFSET(dst, element_size * pindex),
             POINTER_OFFSET(remaining_data, element_size * pindex),
             element_size);
    }

    remaining_data = POINTER_OFFSET(remaining_data, size * element_size);
  }
}

void AttributesRefGroup::set_repeated_elements(uint index,
                                               void *data,
                                               uint data_element_amount,
                                               void *default_value)
{
  if (data_element_amount == 0) {
    this->fill_elements(index, default_value);
    return;
  }

  AttributeType type = m_attributes_info->type_of(index);
  uint element_size = size_of_attribute_type(type);

  uint offset = 0;
  for (AttributesRef attributes : *this) {
    void *dst = attributes.get_ptr(index);

    for (uint pindex = 0; pindex < attributes.size(); pindex++) {
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

void AttributesRefGroup::fill_elements(uint index, void *value)
{
  AttributeType type = m_attributes_info->type_of(index);
  uint element_size = size_of_attribute_type(type);

  for (AttributesRef attributes : *this) {
    void *dst = attributes.get_ptr(index);

    for (uint pindex = 0; pindex < attributes.size(); pindex++) {
      memcpy(POINTER_OFFSET(dst, element_size * pindex), value, element_size);
    }
  }
}

}  // namespace BKE
