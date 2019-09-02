#include "particle_set.hpp"

namespace BParticles {

NewParticles::NewParticles(AttributesInfo &attributes_info,
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

void NewParticles::set_elements(uint index, void *data)
{
  AttributeType type = m_attributes_info->type_of(index);
  uint element_size = size_of_attribute_type(type);

  void *remaining_data = data;

  for (uint i = 0; i < this->range_amount(); i++) {
    AttributeArrays attributes = this->segment(i);
    void *dst = attributes.get_ptr(index);

    uint range_size = m_ranges[i].size();
    for (uint pindex = 0; pindex < range_size; pindex++) {
      memcpy(POINTER_OFFSET(dst, element_size * pindex),
             POINTER_OFFSET(remaining_data, element_size * pindex),
             element_size);
    }

    remaining_data = POINTER_OFFSET(remaining_data, range_size * element_size);
  }
}

void NewParticles::set_repeated_elements(uint index,
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
  for (uint i = 0; i < this->range_amount(); i++) {
    AttributeArrays attributes = this->segment(i);
    void *dst = attributes.get_ptr(index);

    uint range_size = m_ranges[i].size();
    for (uint pindex = 0; pindex < range_size; pindex++) {
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

void NewParticles::fill_elements(uint index, void *value)
{
  AttributeType type = m_attributes_info->type_of(index);
  uint element_size = size_of_attribute_type(type);

  for (uint i = 0; i < this->range_amount(); i++) {
    AttributeArrays attributes = this->segment(i);
    void *dst = attributes.get_ptr(index);

    uint range_size = m_ranges[i].size();
    for (uint pindex = 0; pindex < range_size; pindex++) {
      memcpy(POINTER_OFFSET(dst, element_size * pindex), value, element_size);
    }
  }
}

}  // namespace BParticles
