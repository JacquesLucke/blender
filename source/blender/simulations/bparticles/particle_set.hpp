#pragma once

#include "particles_container.hpp"

namespace BParticles {

class NewParticles {
 private:
  AttributesInfo *m_attributes_info;
  Vector<ArrayRef<void *>> m_buffers;
  Vector<Range<uint>> m_ranges;
  uint m_size;

 public:
  NewParticles(AttributesInfo &attributes_info,
               Vector<ArrayRef<void *>> buffers,
               Vector<Range<uint>> ranges);

  template<typename T> void set(uint index, ArrayRef<T> data)
  {
    BLI_assert(data.size() == m_size);
    BLI_assert(m_attributes_info->type_of(index) == attribute_type_by_type<T>::value);
    this->set_elements(index, (void *)data.begin());
  }

  template<typename T> void set(StringRef name, ArrayRef<T> data)
  {
    uint index = m_attributes_info->attribute_index(name);
    this->set<T>(index, data);
  }

  template<typename T> void set_repeated(uint index, ArrayRef<T> data)
  {
    BLI_assert(m_attributes_info->type_of(index) == attribute_type_by_type<T>::value);
    this->set_repeated_elements(
        index, (void *)data.begin(), data.size(), m_attributes_info->default_value_ptr(index));
  }

  template<typename T> void set_repeated(StringRef name, ArrayRef<T> data)
  {
    uint index = m_attributes_info->attribute_index(name);
    this->set_repeated<T>(index, data);
  }

  template<typename T> void fill(uint index, T value)
  {
    BLI_assert(m_attributes_info->type_of(index) == attribute_type_by_type<T>::value);
    this->fill_elements(index, (void *)&value);
  }

  template<typename T> void fill(StringRef name, T value)
  {
    uint index = m_attributes_info->attribute_index(name);
    this->fill<T>(index, value);
  }

  AttributeArrays segment(uint i)
  {
    return AttributeArrays(*m_attributes_info, m_buffers[i], m_ranges[i]);
  }

  AttributesInfo &attributes_info()
  {
    return *m_attributes_info;
  }

  uint range_amount() const
  {
    return m_buffers.size();
  }

  Range<uint> range(uint i) const
  {
    return m_ranges[i];
  }

 private:
  void set_elements(uint index, void *data);
  void set_repeated_elements(uint index,
                             void *data,
                             uint data_element_amount,
                             void *default_value);
  void fill_elements(uint index, void *value);
};

}  // namespace BParticles
