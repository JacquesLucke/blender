#include "FN_attributes_ref.h"

namespace FN {

void AttributesInfoBuilder::add(const AttributesInfoBuilder &other)
{
  for (uint i = 0; i < other.size(); i++) {
    this->add(other.m_names[i], *other.m_types[i]);
  }
}

void AttributesInfoBuilder::add(const AttributesInfo &other)
{
  for (uint i = 0; i < other.size(); i++) {
    this->add(other.name_of(i), other.type_of(i));
  }
}

AttributesInfo::AttributesInfo(const AttributesInfoBuilder &builder)
{
  for (uint i = 0; i < builder.size(); i++) {
    m_index_by_name.add_new(builder.names()[i], i);
    m_name_by_index.append(builder.names()[i]);
    m_type_by_index.append(builder.types()[i]);
  }
}

AttributesRefGroup::AttributesRefGroup(const AttributesInfo &info,
                                       Vector<ArrayRef<void *>> buffers,
                                       Vector<IndexRange> ranges)
    : m_info(&info), m_buffers(std::move(buffers)), m_ranges(std::move(ranges))
{
  m_total_size = 0;
  for (IndexRange range : m_ranges) {
    m_total_size += range.size();
  }
}

static Array<int> map_attribute_indices(const AttributesInfo &from_info,
                                        const AttributesInfo &to_info)
{
  Array<int> mapping = Array<int>(from_info.size());

  for (uint from_index : from_info.indices()) {
    StringRef name = from_info.name_of(from_index);
    const CPPType &type = from_info.type_of(from_index);

    int to_index = to_info.index_of_try(name, type);
    mapping[from_index] = to_index;
  }

  return mapping;
}

AttributesInfoDiff::AttributesInfoDiff(const AttributesInfo &old_info,
                                       const AttributesInfo &new_info,
                                       const AttributesDefaults &defaults)
    : m_old_info(&old_info), m_new_info(&new_info)
{
  m_old_to_new_mapping = map_attribute_indices(old_info, new_info);
  m_new_to_old_mapping = map_attribute_indices(new_info, old_info);
  m_default_buffers = Array<const void *>(new_info.size(), nullptr);

  for (uint i : new_info.indices()) {
    if (m_new_to_old_mapping[i] >= 0) {
      m_default_buffers[i] = defaults.get(new_info.name_of(i), new_info.type_of(i));
    }
  }
}

void AttributesInfoDiff::update(uint capacity,
                                uint used_size,
                                ArrayRef<void *> old_buffers,
                                MutableArrayRef<void *> new_buffers) const
{
  BLI_assert(old_buffers.size() == m_old_info->size());
  BLI_assert(new_buffers.size() == m_new_info->size());

  for (uint new_index : m_new_info->indices()) {
    int old_index = m_new_to_old_mapping[new_index];
    const CPPType &type = m_new_info->type_of(new_index);

    if (old_index == -1) {
      void *new_buffer = MEM_malloc_arrayN(capacity, type.size(), __func__);

      GenericMutableArrayRef{type, new_buffer, used_size}.fill__uninitialized(
          m_default_buffers[new_index]);

      new_buffers[new_index] = new_buffer;
    }
    else {
      new_buffers[new_index] = old_buffers[old_index];
    }
  };

  for (uint old_index : m_old_info->indices()) {
    int new_index = m_old_to_new_mapping[old_index];

    if (new_index == -1) {
      MEM_freeN(old_buffers[old_index]);
    }
  }
}

}  // namespace FN
