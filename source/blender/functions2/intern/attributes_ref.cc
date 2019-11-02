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

}  // namespace FN
