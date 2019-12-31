#include "FN_attributes_ref.h"

namespace FN {

AttributesInfoBuilder::~AttributesInfoBuilder()
{
  for (uint i = 0; i < m_defaults.size(); i++) {
    m_types[i]->destruct(m_defaults[i]);
  }
}

void AttributesInfoBuilder::add(const AttributesInfoBuilder &other)
{
  for (uint i = 0; i < other.size(); i++) {
    this->add(other.m_names[i], *other.m_types[i], other.m_defaults[i]);
  }
}

void AttributesInfoBuilder::add(const AttributesInfo &other)
{
  for (uint i = 0; i < other.size(); i++) {
    this->add(other.name_of(i), other.type_of(i), other.default_of(i));
  }
}

AttributesInfo::AttributesInfo(const AttributesInfoBuilder &builder)
{
  for (uint i = 0; i < builder.size(); i++) {
    StringRef name = builder.names()[i];
    const CPPType &type = *builder.types()[i];
    const void *default_value = builder.defaults()[i];

    m_index_by_name.add_new(name, i);
    m_name_by_index.append(name);
    m_type_by_index.append(&type);

    void *dst = m_allocator.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(default_value, dst);
    m_defaults.append(dst);
  }
}

AttributesInfo::~AttributesInfo()
{
  for (uint i = 0; i < m_defaults.size(); i++) {
    m_type_by_index[i]->destruct(m_defaults[i]);
  }
}

void MutableAttributesRef::destruct_and_reorder(IndexMask index_mask)
{
#ifdef DEBUG
  BLI_assert(index_mask.size() <= m_range.size());
  BLI_assert(index_mask.size() == 0 || index_mask.last() < m_range.size());
  for (uint i = 1; i < index_mask.size(); i++) {
    BLI_assert(index_mask[i - 1] < index_mask[i]);
  }
#endif

  for (uint attribute_index : m_info->indices()) {
    GenericMutableArrayRef array = this->get(attribute_index);
    const CPPType &type = m_info->type_of(attribute_index);

    array.destruct_indices(index_mask);

    for (uint i : index_mask.index_iterator()) {
      uint last_index = m_range.size() - 1 - i;
      uint index_to_remove = index_mask[index_mask.size() - 1 - i];
      if (index_to_remove == last_index) {
        /* Do nothing. It has been destructed before. */
      }
      else {
        /* Relocate last undestructed value. */
        type.relocate_to_uninitialized(array[last_index], array[index_to_remove]);
      }
    }
  }
}

void MutableAttributesRef::RelocateUninitialized(MutableAttributesRef from,
                                                 MutableAttributesRef to)
{
  BLI_assert(from.size() == to.size());
  BLI_assert(&from.info() == &to.info());

  for (uint attribute_index : from.info().indices()) {
    GenericMutableArrayRef from_array = from.get(attribute_index);
    GenericMutableArrayRef to_array = to.get(attribute_index);

    GenericMutableArrayRef::RelocateUninitialized(from_array, to_array);
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

    int to_index = to_info.try_index_of(name, type);
    mapping[from_index] = to_index;
  }

  return mapping;
}

AttributesInfoDiff::AttributesInfoDiff(const AttributesInfo &old_info,
                                       const AttributesInfo &new_info)
    : m_old_info(&old_info), m_new_info(&new_info)
{
  m_old_to_new_mapping = map_attribute_indices(old_info, new_info);
  m_new_to_old_mapping = map_attribute_indices(new_info, old_info);
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
      void *new_buffer = MEM_mallocN_aligned(capacity * type.size(), type.alignment(), __func__);

      GenericMutableArrayRef{type, new_buffer, used_size}.fill__uninitialized(
          m_new_info->default_of(new_index));

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
